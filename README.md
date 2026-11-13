# AquaTrack

AquaTrack is a native C++ underwater-video review workstation with a Python
model-development toolchain. It is designed to grow from dataset inspection
into detection, tracking, annotation correction, survey analysis, and
reproducible export without tying the product to a browser dashboard.

## Native C++ application

The primary product is a Qt 6 desktop application. Its seven-page shell provides
project management, media references, a four-region video review workspace,
survey analytics, dataset discovery, model-registry inspection, and validated
settings. BrackishMOT sequences play at their recorded frame rate with MOT
ground truth, track filtering, frame metrics, and a catalog-to-review workflow.

On Windows, double-click `start_cpp.bat`, or run:

```powershell
.\build_cpp.bat   # first build, or rebuild after changing C++
.\start_cpp.bat   # launch AquaTrack+
.\test_cpp.bat    # rebuild and run every native test
.\start_cli.bat --help
```

Requirements are Qt 6.7.2 at `C:\Qt\6.7.2\msvc2019_64`, CMake, Ninja, and the
Visual Studio 2022 C++ Build Tools. The build is capped at two workers so it can
coexist with model training. It produces `build\AquaTrack.exe` and the headless
`build\aquatrack-cli.exe`, then deploys the required Qt runtime files.
The QC script uses a separate `build_qc` tree, so tests can run while the normal
desktop executable is open.

In the app, choose **Open sequence** and select a directory containing
`seqinfo.ini`, `img1`, and `gt\gt.txt`, for example:

```text
C:\Users\oyele\Downloads\archive\BrackishMOT\train\brackishMOT-51
```

Run native tests with:

```powershell
.\test_cpp.bat
```

### Video review, detection and tracking

Review opens three kinds of media, all on one frame-numbered timeline:

- **Video files** (MP4, MOV, MKV, AVI, ...) play directly from the original
  file. Frames are decoded on a worker thread by one long-running FFmpeg
  process per clip (1080p sustains >100 fps; seeks are frame-accurate), so
  scrubbing and playback never block the UI. The build copies `ffmpeg.exe`
  and `ffprobe.exe` next to `AquaTrack.exe` when it can find them
  (`winget install Gyan.FFmpeg`).
- **Sequences**: select several videos on the Videos page and choose
  *New sequence*. The videos are referenced, not converted; a
  `sequence.json` in `media/<name>/` lists them in play order. Double-click a
  sequence to browse its clips; double-click a clip to open Review there.
- Existing **BrackishMOT sequences** (`seqinfo.ini`, `img1/`, `gt/gt.txt`) open the same way; a sequence is a sequence whether its frames are stored as video or as images.

**Detect & track** runs the active detector from the project's model registry
(import it from `runs/` on the Models page) with ByteTrack or the IoU tracker,
using the checkout's `.venv` (`python -m aquatrack.analyze`), and loads the
tracks for review as unverified model detections. **Import annotations** loads
ground truth from MOTChallenge `gt.txt` or COCO tracking JSON.

The same steps are scriptable:

```powershell
build\aquatrack-cli.exe project sequence <file.aqp> "Transect A" a.mp4 b.mp4
build\aquatrack-cli.exe project model <file.aqp> runs\detect\<run>\weights\best.pt
build\aquatrack-cli.exe project annotations <file.aqp> media\a.mp4 gt.txt
build\aquatrack-cli.exe project analyze <file.aqp> media\a.mp4 --model <id>
```

SFISHTRACK annotations are made on resampled frame extractions, so annotation
frame k is not MP4 frame k. `tools/sfishtrack_to_mot.py` aligns each annotated
image to its MP4 frame by content and writes a MOTChallenge `gt.txt`; it
refuses to write one when the images do not match the video.

The `src/inference/IDetector.h` interface is the extension point for an
ONNX Runtime or TensorRT detector. Dataset parsing and tracking stay in the C++
core, while Qt owns presentation. Python remains useful for preparing data,
training models, evaluation experiments, and converting trained weights.

## Repository layout

```text
AquaTrack+/
|-- CMakeLists.txt                  # Root native build
|-- CMakePresets.json               # Reproducible configure/build presets
|-- cmake/                          # One build definition per module
|-- apps/{desktop,cli}/             # Desktop and headless entry points
|-- src/
|   |-- application/                # Controllers and workflow state
|   |-- domain/                     # Scientific types and algorithms
|   |-- data/                       # Dataset/file/database adapters
|   |-- inference/                  # Replaceable detector contracts/backends
|   |-- pipeline/                   # Frame and cancellable batch processing
|   |-- analytics/                  # Survey summaries and time bins
|   |-- export/                     # Scientific CSV and MOT exports
|   |-- system/                     # Runtime state, settings, JSONL logs
|   |-- ui/                         # Qt presentation only
|   |   |-- navigation/
|   |   `-- review/{left,center,right}/
|   `-- aquatrack/                  # Python ML and batch-processing package
|-- tools/                          # Dataset preparation and training commands
|-- tests/unit/native/              # C++ unit tests by module
|-- tests/*.py                      # Python package tests
|-- configs/                        # Versioned model/pipeline configuration
|-- build_cpp.bat                   # Native build entry point
|-- test_cpp.bat                    # Build and full native QC
|-- start_cli.bat                   # Headless automation entry point
`-- start_cpp.bat                   # Native application launcher
```

New product UI belongs in its feature directory under `src/ui`, not in
`MainWindow`. Workflows belong in `src/application`; dataset formats belong in
`src/data`; model runtimes belong in `src/inference`; scientific algorithms
without UI or storage concerns belong in `src/domain`. Add sources to the
matching file under `cmake/` so module dependencies remain explicit.

Dependencies point inward: UI uses application controllers; controllers use
data/domain services; pipeline composes detector and tracker contracts; domain
types do not depend on UI or storage. `MainWindow` is only the composition root.

## Native capabilities

- Atomic `.aqp` projects, JSON settings, structured JSONL logs, and a durable,
  schema-versioned scientific observation repository
- BrackishMOT and YOLO dataset catalog with stable IDs and validation warnings
- Detector interface, confidence/geometry filtering, IoU tracking, confirmed
  observation generation, provenance, latency measurement, and cancellation
- Survey summaries, time bins, deterministic CSV/MOT exports, and model manifests
- Native unit, persistence, integration, CLI, and window smoke tests

The C++ detector backend is deliberately replaceable. Python currently owns
training and weight conversion; an ONNX Runtime or TensorRT implementation can
plug into `IDetector` without changing tracking, analytics, storage, or UI code.

## Native CLI

```powershell
.\start_cli.bat datasets "D:\data" --json
.\start_cli.bat project "D:\surveys\harbour.aqp" --json
```

Exit code `0` means success, `1` means invalid usage, and `2` means the requested
project or dataset could not be validated. JSON output is intended for CI,
PowerShell, and downstream scientific workflow automation.

## Python model-development tools

Requires Python 3.11 or newer.

### Manual setup

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -e ".[dev]"
```

Analyze a video:

```powershell
aquatrack path\to\survey.mp4 --output outputs
```

The first run may download the configured YOLO weights. To use custom weights:

```powershell
aquatrack survey.mp4 --weights models\fish_detector.pt --confidence 0.4
```

## Outputs

For `survey.mp4`, AquaTrack writes:

```text
outputs/
|-- survey_annotated.mp4
|-- survey_observations.csv
`-- survey_summary.json
```

Each CSV row contains the frame, timestamp, track ID, class, confidence,
bounding box, and crossing state. The JSON report includes duration, unique
confirmed tracks, line crossings, and per-class totals.

## Configuration

Edit `configs/default.yaml` to select weights, confidence and NMS thresholds,
tracking lifetime, confirmation rules, crossing direction, and output products.

The default class filter is `fish`. For a custom species detector, replace it:

```yaml
model:
  weights: models/species.pt
  classes: [pollock, rockfish, herring]
```

## Architecture

```text
video -> detector -> tracker -> line counter -> observations
                                             |-> annotated video
                                             |-> CSV
                                             `-> JSON summary
```

- `detectors/base.py` defines the detector contract.
- `detectors/yolo.py` adapts Ultralytics results into domain types.
- `tracking.py` contains the deterministic IoU baseline.
- `counting.py` handles directional crossings.
- `pipeline.py` owns video decoding, orchestration, and annotation.
- `export.py` creates analysis-ready outputs.

## Testing

```powershell
pytest
ruff check src tests
```

Core tests do not load neural-network weights. This keeps CI deterministic and
allows tracking and counting behavior to be verified independently.

## Evaluation plan

Use deployment- or video-level train/validation/test splits. Never randomly
split adjacent frames because that leaks nearly identical imagery across sets.

Report:

- Detection precision, recall, and mAP50-95
- Tracking IDF1, HOTA, and identity switches
- Unique-fish count error per video
- Performance by visibility, habitat, depth, and apparent fish size
- Throughput in frames per second on CPU and GPU

The next production milestone is a ByteTrack or BoT-SORT adapter evaluated
against this IoU baseline, followed by an annotation-correction workflow and
fine-tuning on BrackishMOT, SFISHTRACK, or project-specific survey video.

## Prepare BrackishMOT

The downloaded BrackishMOT data contains image sequences and MOTChallenge ground
truth, not ordinary video files. Convert it to a six-class YOLO dataset with:

```powershell
python tools\prepare_brackishmot.py `
  --source "C:\Users\oyele\Downloads\archive\BrackishMOT" `
  --output "C:\Users\oyele\Downloads\archive\BrackishMOT-YOLO"
```

The converter preserves the official test sequences, creates a deterministic
sequence-level validation split from the training sequences, and uses hard links
when possible to avoid duplicating the image data. Keep the prepared output on
the same drive as the source if you want hard links; cross-drive output falls
back to full image copies. It writes:

```text
BrackishMOT-YOLO/
|-- images/{train,val,test}/
|-- labels/{train,val,test}/
|-- dataset.yaml
`-- manifest.json
```

To merge the original `fish` and `small fish` categories into one binary class:

```powershell
python tools\prepare_brackishmot.py `
  --source "C:\Users\oyele\Downloads\archive\BrackishMOT" `
  --output data\brackishmot_fish `
  --fish-only
```

Render the supplied ground truth for a sequence before training:

```powershell
python tools\preview_brackishmot.py `
  --sequence "C:\Users\oyele\Downloads\archive\BrackishMOT\train\brackishMOT-51" `
  --output outputs\brackishMOT-51_ground_truth.mp4 `
  --fish-only
```

Train a YOLO model after preparation:

```powershell
python tools\train_brackishmot.py `
  --data "C:\Users\oyele\Downloads\archive\BrackishMOT-YOLO\dataset.yaml" `
  --epochs 75 `
  --image-size 960 `
  --batch 8 `
  --device 0
```

The best weights are written below `runs/brackishmot/`. Select the resulting
`best.pt` file in AquaTrack's sidebar for video inference.

## Scientific limitations

- A visual detection is an observation, not proof of absolute abundance.
- Camera field of view, animal behavior, visibility, and deployment design affect
  counts.
- Monocular video cannot provide defensible physical length without calibration
  or known scene geometry; stereo cameras are preferred.
- Species predictions should retain an `unknown` option and be reviewed by a
  qualified observer.
- Line crossings estimate passage through a defined image region and should not
  be presented as population size without an appropriate sampling design.

## Data policy

Raw videos, trained weights, and generated outputs are ignored by default because
they may be large or restricted. Record dataset licenses, collection metadata,
annotation provenance, and model versions before publishing derived products.
