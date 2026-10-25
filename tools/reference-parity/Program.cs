using System.Reflection;
using Chess.Core;

const int Infinity = 9_999_999;
const BindingFlags InstancePrivate = BindingFlags.Instance | BindingFlags.NonPublic;

if (args.Length < 2)
{
    Console.Error.WriteLine(
        "Usage: ReferenceParity <eval|moves|hash> <FEN> | search|bench <FEN> <depth> | randomfens|randomaudit <seed> <count> [max-plies]");
    return 2;
}

string action = args[0].ToLowerInvariant();
Board board = action is "randomfens" or "randomaudit"
    ? Board.CreateBoard(FenUtility.StartPositionFEN)
    : Board.CreateBoard(args[1]);

switch (action)
{
    case "eval":
    {
        var evaluator = new Evaluation();
        int score = evaluator.Evaluate(board);
        Console.WriteLine($"eval cp {score}");
        Console.WriteLine($"white material={evaluator.whiteEval.materialScore} pst={evaluator.whiteEval.pieceSquareScore} pawn={evaluator.whiteEval.pawnScore} shield={evaluator.whiteEval.pawnShieldScore} mopup={evaluator.whiteEval.mopUpScore}");
        Console.WriteLine($"black material={evaluator.blackEval.materialScore} pst={evaluator.blackEval.pieceSquareScore} pawn={evaluator.blackEval.pawnScore} shield={evaluator.blackEval.pawnShieldScore} mopup={evaluator.blackEval.mopUpScore}");
        return 0;
    }
    case "moves":
    {
        var generator = new MoveGenerator
        {
            promotionsToGenerate = MoveGenerator.PromotionMode.QueenAndKnight
        };
        Span<Move> moves = stackalloc Move[256];
        generator.GenerateMoves(board, ref moves);
        Console.WriteLine(string.Join(' ', moves.ToArray().Select(MoveToUci)));
        return 0;
    }
    case "hash":
        Console.WriteLine($"hash {board.CurrentGameState.zobristKey}");
        return 0;
    case "randomfens":
    case "randomaudit":
    {
        if (args.Length < 3 || !int.TryParse(args[1], out int seed) ||
            !int.TryParse(args[2], out int sampleCount) || sampleCount < 1)
        {
            Console.Error.WriteLine("randomfens requires a seed and positive sample count");
            return 2;
        }

        int maxPlies = args.Length >= 4 && int.TryParse(args[3], out int parsedMaxPlies)
            ? Math.Max(1, parsedMaxPlies)
            : 80;
        var rng = new Random(seed);
        var generator = new MoveGenerator
        {
            promotionsToGenerate = MoveGenerator.PromotionMode.QueenAndKnight
        };
        for (int sample = 0; sample < sampleCount; sample++)
        {
            Board randomBoard = Board.CreateBoard();
            int targetPlies = rng.Next(maxPlies + 1);
            for (int ply = 0; ply < targetPlies; ply++)
            {
                Span<Move> legalMoves = new Move[256];
                generator.GenerateMoves(randomBoard, ref legalMoves);
                if (legalMoves.Length == 0) break;
                randomBoard.MakeMove(legalMoves[rng.Next(legalMoves.Length)]);
            }
            string randomFen = FenUtility.CurrentFen(randomBoard);
            if (action == "randomaudit")
            {
                var evaluation = new Evaluation();
                int score = evaluation.Evaluate(randomBoard);
                Span<Move> legalMoves = new Move[256];
                generator.GenerateMoves(randomBoard, ref legalMoves);
                string moveList = string.Join(',', legalMoves.ToArray().Select(MoveToUci));
                Console.WriteLine($"{randomFen}|{randomBoard.CurrentGameState.zobristKey}|{score}|{moveList}");
            }
            else
            {
                Console.WriteLine(randomFen);
            }
        }
        return 0;
    }
    case "search":
    case "bench":
    {
        if (args.Length < 3 || !int.TryParse(args[2], out int maxDepth) || maxDepth < 1)
        {
            Console.Error.WriteLine("search requires a positive depth");
            return 2;
        }

        var searcher = new Searcher(board);
        searcher.ClearForNewPosition();
        var repetitionField = typeof(Searcher).GetField("repetitionTable", InstancePrivate)!;
        ((RepetitionTable)repetitionField.GetValue(searcher)!).Init(board);

        MethodInfo search = typeof(Searcher).GetMethod("Search", InstancePrivate)!;
        FieldInfo bestMoveField = typeof(Searcher).GetField("bestMoveThisIteration", InstancePrivate)!;
        FieldInfo bestEvalField = typeof(Searcher).GetField("bestEvalThisIteration", InstancePrivate)!;
        FieldInfo previousMoveField = typeof(Searcher).GetField("bestMove", InstancePrivate)!;
        FieldInfo hasSearchedField = typeof(Searcher).GetField("hasSearchedAtLeastOneMove", InstancePrivate)!;
        FieldInfo nodesField = typeof(Searcher).GetField("nodes", InstancePrivate)!;

        bestMoveField.SetValue(searcher, Move.NullMove);
        bestEvalField.SetValue(searcher, 0);
        previousMoveField.SetValue(searcher, Move.NullMove);

        Move bestMove = Move.NullMove;
        int bestEval = 0;
        var searchTimer = System.Diagnostics.Stopwatch.StartNew();
        for (int depth = 1; depth <= maxDepth; depth++)
        {
            hasSearchedField.SetValue(searcher, false);
            object?[] parameters = { depth, 0, -Infinity, Infinity, 0, Move.NullMove, false };
            bestEval = (int)search.Invoke(searcher, parameters)!;
            bestMove = (Move)bestMoveField.GetValue(searcher)!;
            previousMoveField.SetValue(searcher, bestMove);
            bestMoveField.SetValue(searcher, Move.NullMove);
            bestEvalField.SetValue(searcher, int.MinValue);

            if (Searcher.IsMateScore(bestEval) && Searcher.NumPlyToMateFromScore(bestEval) <= depth)
                break;
        }
        searchTimer.Stop();

        ulong nodesSearched = (ulong)nodesField.GetValue(searcher)!;
        double nps = searchTimer.Elapsed.TotalSeconds > 0
            ? nodesSearched / searchTimer.Elapsed.TotalSeconds
            : 0;
        Console.WriteLine(action == "bench"
            ? $"bestmove {MoveToUci(bestMove)} score {bestEval} nodes {nodesSearched} elapsed_ms {searchTimer.Elapsed.TotalMilliseconds:F3} nps {nps:F0}"
            : $"bestmove {MoveToUci(bestMove)} score {bestEval}");
        return 0;
    }
    default:
        Console.Error.WriteLine($"Unknown action: {action}");
        return 2;
}

static string MoveToUci(Move move)
{
    if (move.IsNull) return "0000";
    string result = BoardHelper.SquareNameFromIndex(move.StartSquare) +
                    BoardHelper.SquareNameFromIndex(move.TargetSquare);
    int promotionType = move.PromotionPieceType;
    if (promotionType != Piece.None)
    {
        result += promotionType switch
        {
            Piece.Queen => "q",
            Piece.Rook => "r",
            Piece.Bishop => "b",
            Piece.Knight => "n",
            _ => string.Empty
        };
    }
    return result;
}
