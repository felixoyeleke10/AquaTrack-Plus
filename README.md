# AquaTrack

AquaTrack is an underwater-video analysis toolkit for detecting, tracking, and
counting fish. It converts survey video into an annotated video, frame-level
observations, and a compact survey summary while keeping detector and tracker
components replaceable for controlled model comparisons.

## What works in this first slice

- YOLO inference through a configurable Ultralytics adapter
- Class-name filtering for fish or custom species models
- Transparent IoU multi-object tracking baseline
- Track confirmation and short-term occlusion tolerance
- Directional line-crossing counts without repeat counting
- Track trails, identities, classes, confidence, and count overlays
- Annotated MP4, observation CSV, and summary JSON outputs
- Streamlit upload/review dashboard
- Unit tests for configuration, association, expiry, and counting
- Docker and GitHub Actions configuration

The default `yolo11n.pt` model is useful for proving the pipeline, but a model
fine-tuned on underwater survey imagery is required for credible field results.

## Quick start

Requires Python 3.11 or newer.

### One-click Windows start

Double-click `start.bat`, or run:

```powershell
.\start.ps1
```

On its first run, the launcher creates `.venv`, installs AquaTrack and the
dashboard dependencies, and opens Streamlit. Later runs reuse the environment.

### Manual setup

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -e ".[dev,dashboard]"
```

Analyze a video:

```powershell
aquatrack path\to\survey.mp4 --output outputs
```

The first run may download the configured YOLO weights. To use custom weights:

```powershell
aquatrack survey.mp4 --weights models\fish_detector.pt --confidence 0.4
```

Launch the dashboard:

```powershell
streamlit run dashboard.py
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
- `dashboard.py` provides an initial scientist-facing review surface.

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
