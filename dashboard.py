from __future__ import annotations

import hashlib
import io
import json
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path

import pandas as pd
import streamlit as st

from aquatrack.config import load_config
from aquatrack.detectors import YoloDetector
from aquatrack.pipeline import process_video


st.set_page_config(page_title="AquaTrack", page_icon=":material/water:", layout="centered")


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def initialize_state() -> None:
    for key, value in {
        "summary": None,
        "video": None,
        "observations": None,
        "manifest": None,
        "source_name": None,
        "review": None,
    }.items():
        st.session_state.setdefault(key, value)


def clear_results() -> None:
    for key in ("summary", "video", "observations", "manifest", "source_name", "review"):
        st.session_state[key] = None


def format_duration(seconds: float) -> str:
    minutes, remainder = divmod(int(seconds), 60)
    return f"{minutes}:{remainder:02d}"


initialize_state()

with st.sidebar:
    st.subheader("Processing")
    weights = st.text_input("Weights", "yolo11n.pt")
    confidence = st.slider("Minimum confidence", 0.05, 0.95, 0.30, 0.05)
    line_position = st.slider("Count line position", 0.10, 0.90, 0.50, 0.05)
    direction = st.segmented_control(
        "Count direction", ["Any", "Up", "Down"], default="Any", width="stretch"
    )
    with st.expander("Tracker"):
        match_iou = st.slider("Match IoU", 0.05, 0.90, 0.30, 0.05)
        confirmation_hits = st.slider("Frames before confirmation", 1, 10, 3)
        missed_frames = st.slider("Frames to keep a lost track", 1, 60, 20)
    if weights == "yolo11n.pt":
        st.caption("Using the general COCO model. Replace this with trained underwater weights for evaluation.")

st.title("AquaTrack")
st.caption("Underwater video review")

uploaded = st.file_uploader(
    "Video",
    type=["mp4", "mov", "avi", "mkv"],
    on_change=clear_results,
)
if uploaded:
    st.caption(f"{uploaded.name} · {uploaded.size / (1024 * 1024):.1f} MB")

process = st.button(
    "Process video",
    type="primary",
    width="stretch",
    disabled=uploaded is None,
)

if process and uploaded is not None:
    try:
        source_bytes = uploaded.getvalue()
        run_id = f"RUN-{uuid.uuid4().hex[:10].upper()}"
        config = load_config("configs/default.yaml")
        config.model.weights = weights
        config.model.confidence = confidence
        config.tracking.match_iou = match_iou
        config.tracking.min_confirmed_hits = confirmation_hits
        config.tracking.max_missed_frames = missed_frames
        config.counting.line_position = line_position
        config.counting.direction = (direction or "Any").lower()

        with st.status("Processing video") as status:
            status.write("Loading model")
            detector = YoloDetector(config.model)
            status.write("Reading frames")

            with tempfile.TemporaryDirectory(prefix="aquatrack-") as temporary:
                work = Path(temporary)
                source = work / uploaded.name
                source.write_bytes(source_bytes)
                result = process_video(source, work / "outputs", detector, config)

                summary = json.loads(result.summary_path.read_text(encoding="utf-8"))
                manifest = {
                    "run_id": run_id,
                    "created_utc": utc_now(),
                    "source": {
                        "filename": uploaded.name,
                        "size_bytes": len(source_bytes),
                        "sha256": hashlib.sha256(source_bytes).hexdigest(),
                    },
                    "model": {"weights": weights, "confidence": confidence},
                    "tracker": {
                        "type": "iou",
                        "match_iou": match_iou,
                        "confirmation_frames": confirmation_hits,
                        "lost_track_frames": missed_frames,
                    },
                    "counting": {
                        "line_position": line_position,
                        "direction": (direction or "Any").lower(),
                    },
                    "results": summary,
                }
                st.session_state.summary = summary
                st.session_state.video = result.video_path.read_bytes()
                st.session_state.observations = result.csv_path.read_bytes()
                st.session_state.manifest = json.dumps(manifest, indent=2).encode("utf-8")
                st.session_state.source_name = uploaded.name
                st.session_state.review = None

            status.update(label="Done", state="complete", expanded=False)
    except Exception as exc:
        st.error(f"Could not process the video: {exc}")

summary = st.session_state.summary
if summary is None:
    st.caption("Start with a short SFISHTRACK or BrackishMOT sequence while testing the pipeline.")
else:
    st.subheader(st.session_state.source_name)
    metrics = st.columns(3)
    metrics[0].metric("Tracks", summary["unique_confirmed_tracks"])
    metrics[1].metric("Crossed line", summary["line_crossing_total"])
    metrics[2].metric("Video length", format_duration(summary["duration_seconds"]))

    st.video(st.session_state.video)

    observations = pd.read_csv(io.BytesIO(st.session_state.observations))
    st.subheader("Detections")
    if observations.empty:
        st.write("No confirmed tracks at the current settings.")
    else:
        st.dataframe(
            observations,
            width="stretch",
            height=360,
            hide_index=True,
            column_config={
                "confidence": st.column_config.ProgressColumn(
                    "Confidence", min_value=0.0, max_value=1.0
                ),
                "timestamp_seconds": st.column_config.NumberColumn("Time (s)", format="%.2f"),
            },
        )

    stem = Path(st.session_state.source_name).stem
    with st.container(horizontal=True):
        st.download_button(
            "Download CSV",
            st.session_state.observations,
            f"{stem}_observations.csv",
            "text/csv",
        )
        st.download_button(
            "Download run JSON",
            st.session_state.manifest,
            f"{stem}_run.json",
            "application/json",
        )

    with st.expander("Review notes and run information"):
        decision = st.segmented_control(
            "Decision", ["Accept", "Needs changes", "Reject"], width="stretch"
        )
        notes = st.text_area("Notes")
        if st.button("Save note"):
            if decision:
                st.session_state.review = {
                    "decision": decision,
                    "notes": notes,
                    "reviewed_utc": utc_now(),
                }
                st.toast("Saved")
            else:
                st.warning("Choose a decision first.")
        if st.session_state.review:
            st.download_button(
                "Download review",
                json.dumps(st.session_state.review, indent=2),
                f"{stem}_review.json",
                "application/json",
            )
        st.json(json.loads(st.session_state.manifest), expanded=False)

st.caption("AquaTrack 0.1")
