#include <condition_variable>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../core/src/image_tracking/common/logger.hpp"
#include "../../test/framework/tracker_config.hpp"
#include "../framework/dataset_adapter.hpp"
#include "../framework/metrics.hpp"

using namespace morpheus::benchmark;
using namespace morpheus::test;

// ============================================================================
// CLI arguments
// ============================================================================

struct Args {
    std::string datasetDir;
    std::string trackerType = "ORB";
    std::string configPath;           // path to the tracker's JSON config (optional)
    std::string outputPath;
    bool verbose = false;
    int jobs = 0;                     // 0 = auto (number of CPU cores)
};

static void printUsage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " --dataset <path> [--tracker ORB|OBD|ShiTomasi] [--config <file.json>]"
                 " [--output <file.csv>] [--jobs N] [--verbose]\n";
}

static Args parseArgs(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--dataset" && i+1 < argc)
            args.datasetDir = argv[++i];
        else if (a == "--tracker" && i+1 < argc)
            args.trackerType = argv[++i];
        else if (a == "--config" && i+1 < argc)
            args.configPath = argv[++i];
        else if (a == "--output" && i+1 < argc)
            args.outputPath = argv[++i];
        else if (a == "--verbose")
            args.verbose = true;
        else if (a == "--jobs" && i+1 < argc)
            args.jobs = std::stoi(argv[++i]);
        else {
            std::cerr << "Unknown argument: " << a << "\n";
            printUsage(argv[0]);
            std::exit(1);
        }
    }
    if (args.datasetDir.empty()) {
        printUsage(argv[0]);
        std::exit(1);
    }
    return args;
}

// ============================================================================
// Runs a single sequence
// ============================================================================

static SequenceResult runSequence(BenchmarkSequence& seq,
                                  TrackerBase& tracker,
                                  bool verbose)
{
    SequenceResult result;
    result.name        = seq.name;
    result.totalFrames = seq.totalFrames;

    tracker.initTemplate(seq.templateImg.data,
                         seq.templateImg.cols,
                         seq.templateImg.rows,
                         true);

    seq.frames->reset();

    cv::Mat frame;
    int frameIdx = 0;

    while (seq.frames->next(frame)) {
        if (frameIdx == 0) {
            // Frame 0 is the template the tracker was already initialized with
            frameIdx++;
            continue;
        }

        tracker.processFrame(frame.data, frame.cols, frame.rows, true);
        HomographyResult hr = tracker.getHomography();

        FrameResult fr;
        fr.frameIndex     = frameIdx;
        fr.trackerSuccess = hr.success;
        fr.hasGT          = seq.gt.count(frameIdx) > 0;

        if (fr.hasGT && hr.success) {
            const FrameGT& gt = seq.gt.at(frameIdx);
            fr.cornerError = computeCornerError(hr.corners, gt.corners);
        }

        if (verbose && fr.hasGT) {
            std::cout << "  [" << seq.name << "] frame " << frameIdx
                      << " success=" << hr.success
                      << " error=" << (fr.cornerError >= 0 ? std::to_string(fr.cornerError) : "N/A")
                      << " px\n";
        }

        result.frames.push_back(fr);
        frameIdx++;
    }

    finalizeSequenceResult(result);
    return result;
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[]) {
    Logger::setLevel(LogLevel::ERROR);

    Args args = parseArgs(argc, argv);

    std::vector<BenchmarkSequence> sequences;
    try {
        sequences = loadFromPOT280(args.datasetDir);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load dataset: " << e.what() << "\n";
        return 1;
    }

    if (sequences.empty()) {
        std::cerr << "No sequences found in: " << args.datasetDir << "\n";
        return 1;
    }

    std::cout << "Loaded " << sequences.size() << " sequences. Tracker: " << args.trackerType << "\n";

    TrackerConfig config;
    if (!args.configPath.empty()) {
        config = TrackerConfig::loadFromFile(args.configPath);
        config.type = args.trackerType;  // --tracker overrides the type from the file
    } else {
        config.type  = args.trackerType;
        config.name  = args.trackerType;
        config.debug = false;
    }
    std::cout << "Config: retryIterations=" << config.trackingParams.retryIterations << "\n";

    std::ofstream outFile;
    std::ostream& out = args.outputPath.empty() ? std::cout : outFile;
    if (!args.outputPath.empty()) {
        outFile.open(args.outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Cannot open output file: " << args.outputPath << "\n";
            return 1;
        }
    }

    int nJobs = args.jobs > 0 ? args.jobs : (int)std::thread::hardware_concurrency();
    if (nJobs <= 0) nJobs = 4;
    std::cout << "Running with " << nJobs << " parallel jobs...\n";

    std::vector<SequenceResult> results(sequences.size());
    std::mutex printMtx;

    // Semaphore built from a counter of in-flight jobs
    std::vector<std::future<void>> futures;
    std::mutex semMtx;
    int activeJobs = 0;
    std::condition_variable semCv;

    for (int i = 0; i < (int)sequences.size(); i++) {
        // Wait for a free slot
        {
            std::unique_lock<std::mutex> lk(semMtx);
            semCv.wait(lk, [&]{ return activeJobs < nJobs; });
            activeJobs++;
        }

        futures.push_back(std::async(std::launch::async, [&, i]() {
            auto& seq = sequences[i];
            {
                std::lock_guard<std::mutex> lk(printMtx);
                std::cout << "Running [" << seq.name << "] (" << seq.totalFrames << " frames)...\n";
            }
            auto tracker = TrackerFactory::createTracker(config);
            try {
                results[i] = runSequence(seq, *tracker, args.verbose);
                std::lock_guard<std::mutex> lk(printMtx);
                std::cout << "  Done [" << seq.name << "] sr="
                          << results[i].successRate << "%\n";
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lk(printMtx);
                std::cerr << "Error in sequence " << seq.name << ": " << e.what() << "\n";
            }
            {
                std::lock_guard<std::mutex> lk(semMtx);
                activeJobs--;
            }
            semCv.notify_one();
        }));
    }

    for (auto& f : futures) f.get();

    // Report results in the original sequence order
    std::vector<SequenceResult> validResults;
    printCSVHeader(out);
    for (auto& r : results) {
        if (!r.name.empty()) {
            printCSVRow(out, r);
            validResults.push_back(r);
        }
    }
    printCSVSummary(out, validResults);

    std::cout << "\nDone. " << validResults.size() << "/" << sequences.size() << " sequences evaluated.\n";
    if (!args.outputPath.empty())
        std::cout << "Results written to: " << args.outputPath << "\n";

    return 0;
}
