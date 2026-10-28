#!/usr/bin/env python3
"""
Ironfang NNUE Trainer (CUDA Accelerated - HalfKAv2 Architecture)
Trained on the Official Lichess Open Evaluations Dataset (Lichess/chess-position-evaluations).
Architecture: HalfKAv2 (45,056 -> 256) x 2 -> 1
Trains directly on NVIDIA GPU (RTX 2050), outputs resources/ironfang.nnue.
"""

import argparse
import math
import os
import struct
import sys
import time
import numpy as np

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    from torch.utils.data import TensorDataset, DataLoader
except ImportError:
    print("Error: PyTorch is required. Install via: py -3.12 -m pip install torch", file=sys.stderr)
    sys.exit(1)

# HalfKAv2 constants: 64 King Squares * 11 Piece Types * 64 Squares = 45,056
NNUE_INPUTS = 45056
PADDING_IDX = 45056
NNUE_HIDDEN = 512
QA = 255
QB = 64
SCALE = 400.0

# HalfKAv2 Piece Types (0..10):
# 0..4: Friendly P, N, B, R, Q
# 5..10: Enemy P, N, B, R, Q, K
WHITE_PIECE_MAP = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4,
    'p': 5, 'n': 6, 'b': 7, 'r': 8, 'q': 9, 'k': 10
}

BLACK_PIECE_MAP = {
    'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4,
    'P': 5, 'N': 6, 'B': 7, 'R': 8, 'Q': 9, 'K': 10
}

def parse_kings(placement):
    """Returns (w_king_sq, b_king_sq) from FEN placement string."""
    w_king = -1
    b_king = -1
    sq = 0
    for ch in placement:
        if ch == '/':
            continue
        elif ch.isdigit():
            sq += int(ch)
        else:
            if ch == 'K':
                w_king = sq
            elif ch == 'k':
                b_king = sq
            sq += 1
    return w_king, b_king

def fen_to_features(fen, max_features=32):
    """
    Parses a single FEN string into HalfKAv2 features for us (friendly) and them (opponent).
    """
    space_pos = fen.find(' ')
    placement = fen[:space_pos] if space_pos != -1 else fen
    stm_white = ' w ' in fen if space_pos != -1 else True

    w_king, b_king = parse_kings(placement)
    if w_king == -1 or b_king == -1:
        return [PADDING_IDX] * max_features, [PADDING_IDX] * max_features

    k_w = w_king
    k_b = b_king ^ 56

    w_features = []
    b_features = []
    sq = 0
    for ch in placement:
        if ch == '/':
            continue
        elif ch.isdigit():
            sq += int(ch)
        else:
            if ch in WHITE_PIECE_MAP:
                p_w = WHITE_PIECE_MAP[ch]
                w_features.append(k_w * 704 + p_w * 64 + sq)

            if ch in BLACK_PIECE_MAP:
                p_b = BLACK_PIECE_MAP[ch]
                sq_b = sq ^ 56
                b_features.append(k_b * 704 + p_b * 64 + sq_b)
            sq += 1

    if stm_white:
        us_idx = w_features
        them_idx = b_features
    else:
        us_idx = b_features
        them_idx = w_features

    us_padded = us_idx[:max_features] + [PADDING_IDX] * max(0, max_features - len(us_idx))
    them_padded = them_idx[:max_features] + [PADDING_IDX] * max(0, max_features - len(them_idx))
    return us_padded, them_padded

class IronfangNNUE(nn.Module):
    def __init__(self, hidden=NNUE_HIDDEN):
        super().__init__()
        self.hidden = hidden
        # HalfKAv2 feature transformer embedding: [45057, hidden] with padding index at 45056
        self.feature_weights = nn.Embedding(NNUE_INPUTS + 1, hidden, padding_idx=PADDING_IDX)
        self.feature_biases = nn.Parameter(torch.zeros(hidden))

        # Output layer: [hidden * 2, 1]
        self.output_weights = nn.Parameter(torch.zeros(hidden * 2))
        self.output_bias = nn.Parameter(torch.zeros(1))

        # Weight initialization
        nn.init.normal_(self.feature_weights.weight, mean=0.0, std=0.02)
        with torch.no_grad():
            self.feature_weights.weight[PADDING_IDX].zero_()
        nn.init.normal_(self.output_weights, mean=0.0, std=0.02)

    def forward(self, us_idx, them_idx):
        # Sparse embedding lookup & sum (feature transformer)
        us = self.feature_biases + self.feature_weights(us_idx).sum(dim=1)
        them = self.feature_biases + self.feature_weights(them_idx).sum(dim=1)

        # Clipped ReLU [0, 1]
        us_act = torch.clamp(us, 0.0, 1.0)
        them_act = torch.clamp(them, 0.0, 1.0)

        # Merge perspectives
        hidden = torch.cat([us_act, them_act], dim=-1)

        # Output evaluation in centipawns
        out = torch.matmul(hidden, self.output_weights) + self.output_bias
        return out

    def export_quantized(self, filepath):
        """
        Exports network weights directly into the binary layout expected by src/nnue.cpp:
        - featureBiases: int16[256]
        - featureWeights: int16[45056][256]
        - outputWeights: int16[512]
        - outputBias: int32
        """
        os.makedirs(os.path.dirname(filepath) or ".", exist_ok=True)
        with open(filepath, "wb") as f:
            # 1. Feature biases: int16[256] scaled by QA
            biases = torch.clamp(self.feature_biases * QA, -32768, 32767).round().to(torch.int16).cpu().numpy()
            f.write(biases.tobytes())

            # 2. Feature weights: int16[45056][256] scaled by QA (excluding padding index)
            weights = torch.clamp(self.feature_weights.weight[:NNUE_INPUTS] * QA, -32768, 32767).round().to(torch.int16).cpu().numpy()
            f.write(weights.tobytes())

            # 3. Output weights: int16[512] scaled by QB
            out_w = torch.clamp(self.output_weights * QB, -32768, 32767).round().to(torch.int16).cpu().numpy()
            f.write(out_w.tobytes())

            # 4. Output bias: int32 scaled by QA * QB
            out_b = int(round(self.output_bias.item() * QA * QB))
            f.write(struct.pack("<i", out_b))

        size = os.path.getsize(filepath)
        print(f"\nSuccessfully exported quantized HalfKAv2 NNUE model to {filepath} ({size:,} bytes)")

def download_dataset(cache_path):
    """
    Downloads the official Lichess Stockfish evaluations dataset shard (data_0000.parquet)
    with resume support and real-time progress reporting.
    """
    os.makedirs(os.path.dirname(cache_path) or ".", exist_ok=True)
    if os.path.isfile(cache_path) and os.path.getsize(cache_path) > 100000000:
        print(f"Using cached Lichess dataset: {cache_path} ({os.path.getsize(cache_path) / 1024 / 1024:.1f} MB)")
        return cache_path

    url = "https://huggingface.co/datasets/Lichess/chess-position-evaluations/resolve/main/data/data_0000.parquet"
    print(f"Downloading official Lichess Stockfish evaluations dataset shard (~2.1 GB)...")
    print(f"Source URL: {url}")
    print(f"Target Path: {cache_path}")

    import requests
    from tqdm import tqdm

    resume_header = {}
    downloaded_bytes = 0
    if os.path.isfile(cache_path):
        downloaded_bytes = os.path.getsize(cache_path)
        resume_header = {'Range': f'bytes={downloaded_bytes}-'}
        print(f"Resuming download from byte {downloaded_bytes:,}...")

    response = requests.get(url, headers=resume_header, stream=True, timeout=30)
    total_size = int(response.headers.get('content-length', 0)) + downloaded_bytes

    mode = 'ab' if downloaded_bytes > 0 else 'wb'
    with open(cache_path, mode) as f, tqdm(
        total=total_size,
        initial=downloaded_bytes,
        unit='iB',
        unit_scale=True,
        unit_divisor=1024,
        desc="Lichess Shard 0000"
    ) as bar:
        for chunk in response.iter_content(chunk_size=1024 * 1024):
            if chunk:
                f.write(chunk)
                bar.update(len(chunk))

    print(f"\nDownload completed: {cache_path} ({os.path.getsize(cache_path) / 1024 / 1024:.1f} MB)")
    return cache_path

def load_and_encode_positions(data_path, count, min_depth=12, max_features=32, start_batch=0,
                              opening_ratio=0.25, middlegame_ratio=0.55, endgame_ratio=0.20):
    """
    Streams Parquet batches and directly populates pre-allocated NumPy buffers.
    Uses balanced 3-phase stratified reservoir sampling:
      - opening_ratio (default 0.25): Openings (>= 26 pieces)
      - middlegame_ratio (default 0.55): Middlegames (12 to 25 pieces)
      - endgame_ratio (default 0.20): Endgames (<= 11 pieces)
    Fully shuffles all 3 pools together before feeding into PyTorch, ensuring every batch
    is an I.I.D. blend across all game phases.
    """
    import pyarrow.parquet as pq

    total_ratio = opening_ratio + middlegame_ratio + endgame_ratio
    opening_ratio /= total_ratio
    middlegame_ratio /= total_ratio
    endgame_ratio /= total_ratio

    target_op = int(count * opening_ratio)
    target_eg = int(count * endgame_ratio)
    target_mg = count - target_op - target_eg

    print(f"Streaming positions from {data_path} (Filter: depth >= {min_depth}, Start Batch: {start_batch})...")
    print(f"Target Distribution: {target_op:,} Openings (>= 26 pcs) + {target_mg:,} Middlegames (12-25 pcs) + {target_eg:,} Endgames (<= 11 pcs)")
    parquet_file = pq.ParquetFile(data_path)
    total_file_rows = parquet_file.metadata.num_rows
    print(f"Dataset shard contains {total_file_rows:,} total evaluations.")
    print(f"Pre-allocating compact buffers for {count:,} positions (HalfKAv2)...")

    us_buf = np.full((count, max_features), PADDING_IDX, dtype=np.int32)
    them_buf = np.full((count, max_features), PADDING_IDX, dtype=np.int32)
    target_buf = np.zeros((count,), dtype=np.float32)

    filled_op = 0
    filled_mg = 0
    filled_eg = 0
    skipped_shallow = 0
    t0 = time.time()

    batch_iter = parquet_file.iter_batches(batch_size=100000, columns=['fen', 'cp', 'mate', 'depth'])
    for batch_idx, batch in enumerate(batch_iter):
        if batch_idx < start_batch:
            continue

        df = batch.to_pandas()
        qual_mask = (df['depth'] >= min_depth) & (df['cp'].notna() | df['mate'].notna())
        df_qual = df[qual_mask]
        skipped_shallow += len(df) - len(df_qual)

        fen_list = df_qual['fen'].tolist()
        cp_list = df_qual['cp'].tolist()
        mate_list = df_qual['mate'].tolist()

        for fen, cp, mate in zip(fen_list, cp_list, mate_list):
            space_pos = fen.find(' ')
            if space_pos == -1:
                continue
            placement = fen[:space_pos]

            # Fast piece count check
            piece_count = sum(1 for ch in placement if ch.isalpha())

            if piece_count >= 26:
                if filled_op >= target_op:
                    continue
                row_idx = filled_op
                filled_op += 1
            elif piece_count <= 11:
                if filled_eg >= target_eg:
                    continue
                row_idx = target_op + filled_eg
                filled_eg += 1
            else:
                if filled_mg >= target_mg:
                    continue
                row_idx = target_op + target_eg + filled_mg
                filled_mg += 1

            stm_white = ' w ' in fen

            # Extract raw evaluation (from White perspective in Lichess dataset)
            if cp is not None and not (isinstance(cp, float) and np.isnan(cp)):
                white_val = float(cp)
            elif mate is not None and not (isinstance(mate, float) and np.isnan(mate)):
                white_val = 4000.0 if mate > 0 else -4000.0
            else:
                continue

            # Convert White perspective to Side-To-Move perspective
            stm_val = white_val if stm_white else -white_val
            cp_clamped = max(-4000.0, min(4000.0, stm_val))
            prob = 1.0 / (1.0 + math.exp(-cp_clamped / SCALE))

            w_king, b_king = parse_kings(placement)
            if w_king == -1 or b_king == -1:
                continue

            k_w = w_king
            k_b = b_king ^ 56

            # Encode features directly into buffer row
            u_idx = 0
            t_idx = 0
            sq = 0
            for ch in placement:
                if ch == '/':
                    continue
                elif ch.isdigit():
                    sq += int(ch)
                else:
                    if ch in WHITE_PIECE_MAP:
                        p_w = WHITE_PIECE_MAP[ch]
                        feat_w = k_w * 704 + p_w * 64 + sq
                        if stm_white:
                            if u_idx < max_features:
                                us_buf[row_idx, u_idx] = feat_w
                                u_idx += 1
                        else:
                            if t_idx < max_features:
                                them_buf[row_idx, t_idx] = feat_w
                                t_idx += 1

                    if ch in BLACK_PIECE_MAP:
                        p_b = BLACK_PIECE_MAP[ch]
                        feat_b = k_b * 704 + p_b * 64 + (sq ^ 56)
                        if stm_white:
                            if t_idx < max_features:
                                them_buf[row_idx, t_idx] = feat_b
                                t_idx += 1
                        else:
                            if u_idx < max_features:
                                us_buf[row_idx, u_idx] = feat_b
                                u_idx += 1

                    sq += 1

            target_buf[row_idx] = prob

            if filled_op >= target_op and filled_mg >= target_mg and filled_eg >= target_eg:
                break

        total_filled = filled_op + filled_mg + filled_eg
        speed = total_filled / max(0.001, time.time() - t0)
        print(f"  Batch {batch_idx + 1:2d} | Op: {filled_op:,}/{target_op:,} | Mg: {filled_mg:,}/{target_mg:,} | Eg: {filled_eg:,}/{target_eg:,} ({speed:,.0f} pos/sec)")
        if filled_op >= target_op and filled_mg >= target_mg and filled_eg >= target_eg:
            break

    total_filled = filled_op + filled_mg + filled_eg
    print(f"\nCollected {total_filled:,} positions in {time.time() - t0:.1f}s.")
    print(f"Shuffling buffers for balanced I.I.D. training distribution...")
    perm = np.random.permutation(total_filled)
    us_buf[:total_filled] = us_buf[perm]
    them_buf[:total_filled] = them_buf[perm]
    target_buf[:total_filled] = target_buf[perm]
    print("Buffers shuffled successfully.")

    return us_buf[:total_filled], them_buf[:total_filled], target_buf[:total_filled]

def test_eval(model, device, fen):
    """Evaluates a test position and returns centipawns."""
    model.eval()
    us, them = fen_to_features(fen)
    with torch.no_grad():
        us_t = torch.tensor([us], dtype=torch.int32, device=device)
        them_t = torch.tensor([them], dtype=torch.int32, device=device)
        cp = model(us_t, them_t).item()
    return cp

def train(args):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("=" * 70)
    print("IRONFANG NNUE NEURAL NETWORK TRAINER (HalfKAv2)")
    print("=" * 70)
    print(f"Architecture: HalfKAv2 ({NNUE_INPUTS:,} inputs -> {args.hidden}x2 -> 1)")
    print(f"Hardware Compute Device: {device.type.upper()}")
    if device.type == "cuda":
        print(f"NVIDIA GPU: {torch.cuda.get_device_name(0)}")
        print(f"VRAM Available: {torch.cuda.get_device_properties(0).total_memory / 1024 / 1024:.0f} MB")

    # Step 1: Load & encode positions into compact numpy buffers
    data_path = args.data if args.data else download_dataset("data/lichess_0000.parquet")
    us_arr, them_arr, target_arr = load_and_encode_positions(
        data_path, args.count, args.min_depth,
        start_batch=args.start_batch,
        opening_ratio=args.opening_ratio,
        middlegame_ratio=args.middlegame_ratio,
        endgame_ratio=args.endgame_ratio
    )

    # Step 2: Build PyTorch Dataset (zero-copy from numpy)
    print("Transferring tensors to PyTorch DataLoader...")
    dataset = TensorDataset(
        torch.from_numpy(us_arr),
        torch.from_numpy(them_arr),
        torch.from_numpy(target_arr)
    )
    loader = DataLoader(dataset, batch_size=args.batch_size, shuffle=True, drop_last=(len(dataset) > args.batch_size))

    # Step 3: Initialize model, optimizer, scheduler & loss
    model = IronfangNNUE(hidden=args.hidden).to(device)
    if args.resume and os.path.isfile(args.resume):
        print(f"Resuming weights from checkpoint: {args.resume}")
        model.load_state_dict(torch.load(args.resume, map_location=device))

    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.epochs, eta_min=1e-5)
    criterion = nn.MSELoss()

    print(f"\nStarting GPU Training: {len(dataset):,} positions | {args.epochs} epochs | Batch Size: {args.batch_size}")
    print("-" * 70)

    start_time = time.time()
    for epoch in range(1, args.epochs + 1):
        epoch_start = time.time()
        total_loss = 0.0
        batches = 0

        model.train()
        for us_idx, them_idx, targets in loader:
            us_idx = us_idx.to(device)
            them_idx = them_idx.to(device)
            targets = targets.to(device)

            optimizer.zero_grad()
            outputs = model(us_idx, them_idx).view(-1)
            # Sigmoid activation to match win probability [0, 1]
            pred_prob = torch.sigmoid(outputs / SCALE)
            loss = criterion(pred_prob, targets)

            loss.backward()
            optimizer.step()

            total_loss += loss.item()
            batches += 1

        scheduler.step()
        avg_loss = total_loss / max(1, batches)
        elapsed = time.time() - epoch_start
        pos_per_sec = (len(dataset)) / max(0.001, elapsed)

        # Check sample positions across game phases to see learning in action
        cp_start = test_eval(model, device, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        cp_up_q = test_eval(model, device, "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        cp_dn_q = test_eval(model, device, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1")
        # Test true Ruy Lopez move 5 positions (White perspective)
        cp_ruy_e7 = test_eval(model, device, "r1bqk2r/1pppbppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1RK1 w kq - 4 6")
        cp_ruy_g8 = test_eval(model, device, "r1bqkbr1/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQ1RK1 w q - 4 6")
        # Test endgame positions: King Opposition (K+P vs K) and Rook Endgame
        cp_kpk = test_eval(model, device, "8/4k3/8/4P3/4K3/8/8/8 w - - 0 1")
        cp_krpkr = test_eval(model, device, "8/2R5/8/4k3/8/8/4K3/r7 w - - 0 1")

        print(f"Epoch {epoch:2d}/{args.epochs:2d} | Loss: {avg_loss:.5f} | Speed: {pos_per_sec:,.0f} pos/sec | Time: {elapsed:.1f}s")
        print(f"   Middlegame: Start: {cp_start:+.0f} cp | +Queen: {cp_up_q:+.0f} cp | -Queen: {cp_dn_q:+.0f} cp")
        print(f"   Opening:    5...Be7: {cp_ruy_e7:+.0f} cp | 5...Rg8: {cp_ruy_g8:+.0f} cp")
        print(f"   Endgame:    K+P vs K: {cp_kpk:+.0f} cp | Rook Ending: {cp_krpkr:+.0f} cp")

    total_time = time.time() - start_time
    print("-" * 70)
    print(f"Training completed in {total_time:.1f}s ({total_time / 60:.1f} mins)!")

    # Step 4: Export quantized network for Ironfang C++
    model.export_quantized(args.output)
    # Also save PyTorch checkpoint for future training/resuming
    chk_path = args.checkpoint if hasattr(args, "checkpoint") and args.checkpoint else "resources/checkpoint.pt"
    torch.save(model.state_dict(), chk_path)
    print(f"Saved PyTorch checkpoint to {chk_path} (ready for future fine-tuning!)")

def main():
    parser = argparse.ArgumentParser(description="Train Ironfang HalfKAv2 NNUE Network on Lichess Dataset")
    parser.add_argument("--hidden", type=int, default=512, help="Hidden layer size (default: 512)")
    parser.add_argument("--count", type=int, default=10000000, help="Number of positions to train on (default: 10,000,000)")
    parser.add_argument("--start-batch", type=int, default=0, help="Batch index to start streaming from (default: 0)")
    parser.add_argument("--opening-ratio", type=float, default=0.25, help="Target ratio of openings (>= 26 pieces) (default: 0.25)")
    parser.add_argument("--middlegame-ratio", type=float, default=0.55, help="Target ratio of middlegames (12..25 pieces) (default: 0.55)")
    parser.add_argument("--endgame-ratio", type=float, default=0.20, help="Target ratio of endgames (<= 11 pieces) (default: 0.20)")
    parser.add_argument("--min-depth", type=int, default=12, help="Minimum Stockfish evaluation depth to accept (default: 12 plies)")
    parser.add_argument("--epochs", type=int, default=5, help="Number of training epochs (default: 5)")
    parser.add_argument("--batch-size", type=int, default=8192, help="Batch size for GPU training (default: 8192)")
    parser.add_argument("--lr", type=float, default=0.001, help="Learning rate (default: 0.001)")
    parser.add_argument("--data", type=str, default=None, help="Path to local parquet file (optional)")
    parser.add_argument("--resume", type=str, default=None, help="Resume from previous checkpoint .pt")
    parser.add_argument("--checkpoint", type=str, default="resources/checkpoint.pt", help="Checkpoint save path (default: resources/checkpoint.pt)")
    parser.add_argument("--output", type=str, default="resources/ironfang.nnue", help="Output .nnue binary path")
    args = parser.parse_args()

    train(args)

if __name__ == "__main__":
    main()
