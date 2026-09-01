from __future__ import annotations

import json
import tempfile
from pathlib import Path

import pandas as pd
import streamlit as st

from aquatrack.config import load_config
from aquatrack.detectors import YoloDetector
from aquatrack.pipeline import process_video


st.set_page_config(page_title="AquaTrack", page_icon="🐟", layout="wide")
st.title("AquaTrack")
st.caption("Underwater fish detection, tracking, counting, and survey review")

with st.sidebar:
    st.header("Inference settings")
    weights = st.text_input("YOLO weights", "yolo11n.pt")
    confidence = st.slider("Detection confidence", 0.05, 0.95, 0.30, 0.05)
    line_position = st.slider("Counting-line position", 0.10, 0.90, 0.50, 0.05)
    direction = st.selectbox("Crossing direction", ["any", "up", "down"])

uploaded = st.file_uploader("Upload an underwater video", type=["mp4", "mov", "avi", "mkv"])
if uploaded and st.button("Analyze video", type="primary"):
    with tempfile.TemporaryDirectory(prefix="aquatrack-") as temporary:
        work = Path(temporary)
        source = work / uploaded.name
        source.write_bytes(uploaded.getbuffer())
        config = load_config("configs/default.yaml")
        config.model.weights = weights
        config.model.confidence = confidence
        config.counting.line_position = line_position
        config.counting.direction = direction
        with st.status("Analyzing video…", expanded=True) as status:
            st.write("Loading detector")
            detector = YoloDetector(config.model)
            st.write("Running detection and tracking")
            result = process_video(source, work / "outputs", detector, config)
            status.update(label="Analysis complete", state="complete")

        summary = json.loads(result.summary_path.read_text(encoding="utf-8"))
        col1, col2, col3 = st.columns(3)
        col1.metric("Unique tracks", summary["unique_confirmed_tracks"])
        col2.metric("Line crossings", summary["line_crossing_total"])
        col3.metric("Frames", summary["frames_processed"])
        st.video(result.video_path.read_bytes())
        observations = pd.read_csv(result.csv_path)
        st.subheader("Observations")
        st.dataframe(observations, use_container_width=True, hide_index=True)
        st.download_button("Download observations CSV", result.csv_path.read_bytes(),
                           result.csv_path.name, "text/csv")
        st.download_button("Download summary JSON", result.summary_path.read_bytes(),
                           result.summary_path.name, "application/json")

