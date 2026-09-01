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


st.set_page_config(
    page_title="AquaTrack",
    page_icon=":material/water:",
    layout="centered",
    initial_sidebar_state="expanded",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def initialize_state() -> None:
    defaults = {
        "summary": None,
        "video": None,
        "observations": None,
        "manifest": None,
        "source_name": None,
        "review": None,
    }
    for key, value in defaults.items():
        st.session_state.setdefault(key, value)


def clear_results() -> None:
    for key in ("summary", "video", "observations", "manifest", "source_name", "review"):
        st.session_state[key] = None


def format_duration(seconds: float) -> str:
    minutes, remainder = divmod(int(seconds), 60)
    return f"{minutes:02d}:{remainder:02d}"


initialize_state()

with st.sidebar:
    st.header("Settings")
    weights = st.text_input(
        "Model",
        "yolo11n.pt",
        help="Select custom underwater weights when they are available.",
    )
    confidence = st.slider("Confidence", 0.05, 0.95, 0.30, 0.05)
    line_position = st.slider("Counting line", 0.10, 0.90, 0.50, 0.05)
    direction = st.segmented_control(
        "Direction",
        ["Any", "Up", "Down"],
        default="Any",
        width="stretch",
    )

    with st.expander("Advanced tracking", icon=":material/tune:"):
        match_iou = st.slider("Association IoU", 0.05, 0.90, 0.30, 0.05)
        confirmation_hits = st.slider("Confirmation frames", 1, 10, 3)
        missed_frames = st.slider("Occlusion tolerance", 1, 60, 20)

    st.caption("AquaTrack 0.1 · Research preview")

st.title("AquaTrack")
st.write("Detect, track, and count fish in underwater video.")

uploaded = st.file_uploader(
    "Upload a video",
    type=["mp4", "mov", "avi", "mkv"],
    on_change=clear_results,
)

if uploaded:
    st.caption(f"{uploaded.name} · {uploaded.size / (1024 * 1024):.1f} MB")

analyze = st.button(
    "Analyze video",
    type="primary",
    icon=":material/play_arrow:",
    width="stretch",
    disabled=uploaded is None,
)

if analyze and uploaded is not None:
    try:
        source_bytes = uploaded.getvalue()
        run_id = f"RUN-{uuid.uuid4().hex[:10].upper()}"

        with st.status("Analyzing video", expanded=True) as status:
            status.write("Loading model")
            config = load_config("configs/default.yaml")
            config.model.weights = weights
            config.model.confidence = confidence
            config.tracking.match_iou = match_iou
            config.tracking.min_confirmed_hits = confirmation_hits
            config.tracking.max_missed_frames = missed_frames
            config.counting.line_position = line_position
            config.counting.direction = (direction or "Any").lower()
            detector = YoloDetector(config.model)

            status.write("Detecting and tracking fish")
            with tempfile.TemporaryDirectory(prefix="aquatrack-") as temporary:
                work = Path(temporary)
                source = work / uploaded.name
                source.write_bytes(source_bytes)
                result = process_video(source, work / "outputs", detector, config)

                summary = json.loads(result.summary_path.read_text(encoding="utf-8"))
                manifest = {
                    "schema": "aquatrack.run-manifest.v1",
                    "run_id": run_id,
                    "created_utc": utc_now(),
                    "source": {
                        "filename": uploaded.name,
                        "size_bytes": len(source_bytes),
                        "sha256": hashlib.sha256(source_bytes).hexdigest(),
                    },
                    "model": {
                        "weights": weights,
                        "confidence": confidence,
                        "validation": "baseline" if weights == "yolo11n.pt" else "custom",
                    },
                    "tracking": {
                        "method": "greedy_iou_baseline",
                        "match_iou": match_iou,
                        "confirmation_frames": confirmation_hits,
                        "max_missed_frames": missed_frames,
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

            status.update(label="Analysis complete", state="complete", expanded=False)
    except Exception as exc:
        st.error(f"Analysis failed: {exc}", icon=":material/error:")

summary = st.session_state.summary
if summary is None:
    st.space("medium")
    with st.container(border=True):
        st.subheader("How it works")
        st.write("1. Upload an underwater video.")
        st.write("2. AquaTrack detects fish and follows them across frames.")
        st.write("3. Review the annotated video and download the results.")
        st.caption("The default model is a general-purpose baseline. Use trained underwater weights for scientific evaluation.")
else:
    st.space("medium")
    st.subheader("Results")
    metric_columns = st.columns(4)
    metric_columns[0].metric("Fish tracks", summary["unique_confirmed_tracks"])
    metric_columns[1].metric("Crossings", summary["line_crossing_total"])
    metric_columns[2].metric("Duration", format_duration(summary["duration_seconds"]))
    metric_columns[3].metric("Frames", f'{summary["frames_processed"]:,}')

    st.video(st.session_state.video)
    st.caption("Green boxes are active tracks. Amber boxes have crossed the counting line.")

    observations = pd.read_csv(io.BytesIO(st.session_state.observations))
    data_tab, review_tab = st.tabs(["Observations", "Review"])

    with data_tab:
        if observations.empty:
            st.info("No confirmed fish tracks were found at the current settings.")
        else:
            st.dataframe(
                observations,
                width="stretch",
                hide_index=True,
                column_config={
                    "confidence": st.column_config.ProgressColumn(
                        "Confidence", min_value=0.0, max_value=1.0
                    ),
                    "timestamp_seconds": st.column_config.NumberColumn("Time (s)", format="%.2f"),
                },
            )

    with review_tab:
        decision = st.segmented_control(
            "Review decision",
            ["Accept", "Needs changes", "Reject"],
            width="stretch",
        )
        notes = st.text_area("Notes", placeholder="Record false detections or other observations")
        if st.button("Save review", icon=":material/check:"):
            if not decision:
                st.warning("Choose a review decision first.")
            else:
                st.session_state.review = {
                    "decision": decision,
                    "notes": notes,
                    "reviewed_utc": utc_now(),
                }
                st.toast("Review saved", icon=":material/check_circle:")

    with st.expander("Downloads and run details", icon=":material/download:"):
        stem = Path(st.session_state.source_name).stem
        with st.container(horizontal=True):
            st.download_button(
                "Observations CSV",
                st.session_state.observations,
                f"{stem}_observations.csv",
                "text/csv",
                icon=":material/table_view:",
            )
            st.download_button(
                "Run manifest",
                st.session_state.manifest,
                f"{stem}_manifest.json",
                "application/json",
                icon=":material/description:",
            )
            if st.session_state.review:
                st.download_button(
                    "Review JSON",
                    json.dumps(st.session_state.review, indent=2),
                    f"{stem}_review.json",
                    "application/json",
                    icon=":material/fact_check:",
                )
        if weights == "yolo11n.pt":
            st.warning(
                "This run used generic model weights. Treat results as a pipeline demonstration, not a validated scientific product.",
                icon=":material/warning:",
            )
        st.json(json.loads(st.session_state.manifest), expanded=False)

st.space("large")
st.caption("AquaTrack · Underwater video analysis · Verify model outputs before scientific use")
