from __future__ import annotations

import json
import tempfile
from pathlib import Path

import pandas as pd
import streamlit as st

from aquatrack.config import load_config
from aquatrack.detectors import YoloDetector
from aquatrack.pipeline import process_video


st.set_page_config(
    page_title="AquaTrack | Marine Video Intelligence",
    page_icon="🐟",
    layout="wide",
    initial_sidebar_state="expanded",
)


THEME = """
<style>
@import url('https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;500;600;700&family=Manrope:wght@600;700;800&display=swap');

:root {
    --ocean-950: #041d2b;
    --ocean-900: #082b3a;
    --ocean-700: #0b5a6b;
    --ocean-500: #0d8f9b;
    --aqua-400: #35d2c4;
    --foam: #eefbf9;
    --ink: #102a35;
    --muted: #617781;
    --line: #dbe8e9;
}

html, body, [class*="css"] { font-family: 'DM Sans', sans-serif; }
h1, h2, h3 { font-family: 'Manrope', sans-serif !important; letter-spacing: -0.035em; }
.stApp { background: linear-gradient(180deg, #f7fbfb 0%, #ffffff 38%); color: var(--ink); }
[data-testid="stHeader"] { background: transparent; }
[data-testid="stSidebar"] { background: var(--ocean-950); border-right: 1px solid #123c49; }
[data-testid="stSidebar"] * { color: #e8f8f6; }
[data-testid="stSidebar"] .stSlider [data-baseweb="slider"] div { color: var(--aqua-400); }
[data-testid="stSidebar"] hr { border-color: #244552; }
.block-container { max-width: 1440px; padding-top: 1.4rem; padding-bottom: 4rem; }

.brand { display: flex; align-items: center; gap: .75rem; padding: .35rem 0 1.4rem; }
.brand-mark { width: 42px; height: 42px; border-radius: 13px; display: grid; place-items: center;
    background: linear-gradient(145deg, #45e0cf, #1499ad); color: #03202c; font-size: 1.35rem;
    box-shadow: 0 8px 26px rgba(25, 198, 187, .22); }
.brand-name { font: 800 1.15rem/1 'Manrope', sans-serif; color: white; }
.brand-sub { color: #82aab2; font-size: .72rem; margin-top: .28rem; letter-spacing: .06em; text-transform: uppercase; }

.hero { position: relative; overflow: hidden; padding: 2.4rem 2.6rem; border-radius: 24px;
    background: linear-gradient(120deg, #052b3b 0%, #075260 58%, #078a8e 100%);
    color: white; box-shadow: 0 22px 60px rgba(4, 45, 57, .16); margin-bottom: 1.5rem; }
.hero:after { content: ''; position: absolute; width: 380px; height: 380px; right: -100px; top: -190px;
    border-radius: 50%; border: 70px solid rgba(102, 239, 220, .09); }
.eyebrow { display: inline-flex; align-items: center; gap: .45rem; padding: .35rem .7rem;
    border: 1px solid rgba(166, 243, 233, .28); border-radius: 999px; background: rgba(0,0,0,.12);
    color: #baf5ed; font-weight: 600; font-size: .72rem; letter-spacing: .08em; text-transform: uppercase; }
.hero h1 { color: white; font-size: clamp(2.1rem, 4vw, 3.5rem); max-width: 780px; margin: .9rem 0 .55rem; line-height: 1.03; }
.hero p { color: #cce7e8; max-width: 730px; font-size: 1rem; line-height: 1.65; margin: 0; }
.hero-meta { display: flex; flex-wrap: wrap; gap: 1rem; margin-top: 1.35rem; color: #a9d4d5; font-size: .82rem; }

.section-title { margin: 1.8rem 0 .25rem; font: 700 1.28rem/1.2 'Manrope', sans-serif; color: var(--ink); }
.section-copy { color: var(--muted); margin-bottom: 1rem; font-size: .91rem; }
.panel { background: white; border: 1px solid var(--line); border-radius: 18px; padding: 1.2rem 1.3rem;
    box-shadow: 0 9px 30px rgba(11, 67, 78, .055); height: 100%; }
.step-number { width: 28px; height: 28px; border-radius: 9px; background: #e5f8f5; color: #087d7e;
    display: inline-grid; place-items: center; font-weight: 800; font-size: .76rem; margin-bottom: .75rem; }
.step-title { font-weight: 700; color: var(--ink); margin-bottom: .35rem; }
.step-copy { color: var(--muted); font-size: .84rem; line-height: 1.55; }

[data-testid="stFileUploader"] { background: white; border: 1px dashed #8ebdc0; border-radius: 18px; padding: .65rem; }
[data-testid="stFileUploader"] section { border: none; background: #f5fbfa; border-radius: 13px; }
.stButton > button { border-radius: 12px; min-height: 3rem; font-weight: 700; border: 0;
    background: linear-gradient(100deg, #087b87, #0ba59b); color: white; box-shadow: 0 8px 20px rgba(9, 143, 146, .17); }
.stButton > button:hover { color: white; border: 0; transform: translateY(-1px); }

[data-testid="stMetric"] { background: white; border: 1px solid var(--line); padding: 1.05rem 1.15rem;
    border-radius: 16px; box-shadow: 0 7px 24px rgba(10, 68, 77, .05); }
[data-testid="stMetricLabel"] { color: var(--muted); }
[data-testid="stMetricValue"] { color: var(--ocean-900); font-family: 'Manrope', sans-serif; }
[data-testid="stTabs"] button { font-weight: 700; }
[data-testid="stDataFrame"] { border: 1px solid var(--line); border-radius: 14px; overflow: hidden; }
.quality-note { padding: .85rem 1rem; border-radius: 12px; background: #fff8e6; border: 1px solid #f1dda1;
    color: #725b1d; font-size: .84rem; line-height: 1.5; }
.footer { margin-top: 3rem; padding-top: 1.2rem; border-top: 1px solid var(--line); display: flex;
    justify-content: space-between; color: #789098; font-size: .78rem; }
</style>
"""
st.markdown(THEME, unsafe_allow_html=True)


def initialize_state() -> None:
    defaults = {
        "analysis": None,
        "annotated_video": None,
        "observations_csv": None,
        "summary_json": None,
        "source_name": None,
    }
    for key, value in defaults.items():
        st.session_state.setdefault(key, value)


def clear_analysis() -> None:
    for key in ("analysis", "annotated_video", "observations_csv", "summary_json", "source_name"):
        st.session_state[key] = None


def format_duration(seconds: float) -> str:
    minutes, remainder = divmod(int(seconds), 60)
    return f"{minutes:02d}:{remainder:02d}"


initialize_state()

with st.sidebar:
    st.markdown(
        """<div class="brand"><div class="brand-mark">◉</div><div><div class="brand-name">AquaTrack</div>
        <div class="brand-sub">Marine video intelligence</div></div></div>""",
        unsafe_allow_html=True,
    )
    st.caption("ANALYSIS CONFIGURATION")
    weights = st.text_input(
        "Model weights",
        "yolo11n.pt",
        help="Use custom underwater weights for scientifically meaningful detections.",
    )
    confidence = st.slider("Detection confidence", 0.05, 0.95, 0.30, 0.05)
    with st.expander("Tracking", expanded=True):
        match_iou = st.slider("Association IoU", 0.05, 0.90, 0.30, 0.05)
        confirmation_hits = st.slider("Confirmation frames", 1, 10, 3)
        missed_frames = st.slider("Occlusion tolerance", 1, 60, 20)
    with st.expander("Counting", expanded=True):
        line_position = st.slider("Counting-line position", 0.10, 0.90, 0.50, 0.05)
        direction = st.selectbox("Crossing direction", ["any", "up", "down"])
    st.divider()
    st.caption("MODEL STATUS")
    if weights == "yolo11n.pt":
        st.warning("Generic COCO baseline\n\nFine-tune before reporting scientific performance.")
    else:
        st.success(f"Custom weights selected\n\n`{Path(weights).name}`")
    st.caption("AquaTrack 0.1.0 · Research preview")

st.markdown(
    """
    <section class="hero">
      <span class="eyebrow">● Computer vision for ocean observation</span>
      <h1>Turn underwater video into reviewable marine observations.</h1>
      <p>Detect, track and count fish across survey footage. Preserve frame-level evidence,
      quantify uncertainty and export transparent data products for scientific review.</p>
      <div class="hero-meta"><span>▣ Video analytics</span><span>◎ Persistent identities</span>
      <span>↗ Reproducible exports</span><span>✓ Human-review ready</span></div>
    </section>
    """,
    unsafe_allow_html=True,
)

st.markdown('<div class="section-title">Analysis workflow</div>', unsafe_allow_html=True)
st.markdown(
    '<div class="section-copy">A transparent pipeline from source footage to auditable observations.</div>',
    unsafe_allow_html=True,
)
workflow_columns = st.columns(4)
steps = [
    ("01", "Upload", "Add field, ROV, drop-camera or laboratory video."),
    ("02", "Detect", "Localize candidate fish with configurable model thresholds."),
    ("03", "Track", "Associate observations through time and prevent repeat counts."),
    ("04", "Review", "Inspect evidence, QC context and export analysis-ready results."),
]
for column, (number, title, copy) in zip(workflow_columns, steps):
    column.markdown(
        f'<div class="panel"><div class="step-number">{number}</div><div class="step-title">{title}</div>'
        f'<div class="step-copy">{copy}</div></div>',
        unsafe_allow_html=True,
    )

st.markdown('<div class="section-title">New video analysis</div>', unsafe_allow_html=True)
st.markdown(
    '<div class="section-copy">Supported containers: MP4, MOV, AVI and MKV. Keep raw survey footage unchanged.</div>',
    unsafe_allow_html=True,
)
upload_column, context_column = st.columns([1.65, 1], gap="large")
with upload_column:
    uploaded = st.file_uploader(
        "Survey video",
        type=["mp4", "mov", "avi", "mkv"],
        label_visibility="collapsed",
        on_change=clear_analysis,
    )
    analyze = st.button("Run video analysis", type="primary", use_container_width=True, disabled=uploaded is None)

with context_column:
    st.markdown(
        """<div class="panel"><div class="step-title">Before you analyze</div>
        <div class="step-copy">Record the deployment, camera, timestamp, location, depth and visibility
        alongside the source video. AquaTrack outputs observations—not absolute population abundance—and
        every model result should remain traceable to its source.</div></div>""",
        unsafe_allow_html=True,
    )

if analyze and uploaded is not None:
    try:
        with tempfile.TemporaryDirectory(prefix="aquatrack-") as temporary:
            work = Path(temporary)
            source = work / uploaded.name
            source.write_bytes(uploaded.getbuffer())
            config = load_config("configs/default.yaml")
            config.model.weights = weights
            config.model.confidence = confidence
            config.tracking.match_iou = match_iou
            config.tracking.min_confirmed_hits = confirmation_hits
            config.tracking.max_missed_frames = missed_frames
            config.counting.line_position = line_position
            config.counting.direction = direction

            progress = st.progress(0, text="Preparing analysis environment")
            progress.progress(12, text="Loading detection model")
            detector = YoloDetector(config.model)
            progress.progress(28, text="Decoding video and tracking observations")
            result = process_video(source, work / "outputs", detector, config)
            progress.progress(92, text="Packaging scientific outputs")

            st.session_state.analysis = json.loads(result.summary_path.read_text(encoding="utf-8"))
            st.session_state.annotated_video = result.video_path.read_bytes()
            st.session_state.observations_csv = result.csv_path.read_bytes()
            st.session_state.summary_json = result.summary_path.read_bytes()
            st.session_state.source_name = uploaded.name
            progress.progress(100, text="Analysis complete")
    except Exception as exc:
        st.error(f"Analysis failed: {exc}")

summary = st.session_state.analysis
if summary:
    st.markdown('<div class="section-title">Survey results</div>', unsafe_allow_html=True)
    st.markdown(
        f'<div class="section-copy">Analysis summary for <strong>{st.session_state.source_name}</strong></div>',
        unsafe_allow_html=True,
    )
    metrics = st.columns(5)
    metrics[0].metric("Unique tracks", summary["unique_confirmed_tracks"])
    metrics[1].metric("Line crossings", summary["line_crossing_total"])
    metrics[2].metric("Frames analyzed", f'{summary["frames_processed"]:,}')
    metrics[3].metric("Duration", format_duration(summary["duration_seconds"]))
    metrics[4].metric("Source FPS", f'{summary["source_fps"]:.1f}')

    observations = pd.read_csv(pd.io.common.BytesIO(st.session_state.observations_csv))
    overview_tab, evidence_tab, data_tab, export_tab = st.tabs(
        ["Overview", "Video evidence", "Observations", "Export & provenance"]
    )
    with overview_tab:
        left, right = st.columns([1.2, 1], gap="large")
        with left:
            st.subheader("Track composition")
            composition = summary.get("unique_tracks_by_class", {})
            if composition:
                chart_data = pd.DataFrame(
                    {"Class": composition.keys(), "Unique tracks": composition.values()}
                ).set_index("Class")
                st.bar_chart(chart_data, color="#0b9e98")
            else:
                st.info("No confirmed tracks met the configured thresholds.")
        with right:
            st.subheader("Quality context")
            st.markdown(
                """<div class="quality-note"><strong>Research preview:</strong> Counts depend on camera
                placement, field of view, visibility, animal behavior and model calibration. Review the
                annotated video and uncertain observations before interpreting results.</div>""",
                unsafe_allow_html=True,
            )
            st.write("")
            st.json(
                {
                    "model": Path(weights).name,
                    "confidence_threshold": confidence,
                    "tracking_iou": match_iou,
                    "crossing_direction": direction,
                },
                expanded=True,
            )
    with evidence_tab:
        st.video(st.session_state.annotated_video)
        st.caption("Green tracks are active observations; amber tracks have crossed the counting line.")
    with data_tab:
        filter_classes = sorted(observations["class_name"].dropna().unique().tolist()) if not observations.empty else []
        selected = st.multiselect("Filter classes", filter_classes, default=filter_classes)
        filtered = observations[observations["class_name"].isin(selected)] if selected else observations.iloc[0:0]
        st.dataframe(
            filtered,
            use_container_width=True,
            hide_index=True,
            column_config={
                "confidence": st.column_config.ProgressColumn("Confidence", min_value=0.0, max_value=1.0),
                "timestamp_seconds": st.column_config.NumberColumn("Time (s)", format="%.2f"),
            },
        )
    with export_tab:
        st.subheader("Analysis package")
        st.write("Download open, machine-readable products for review or downstream integration.")
        first, second = st.columns(2)
        first.download_button(
            "Download observations CSV",
            st.session_state.observations_csv,
            f"{Path(st.session_state.source_name).stem}_observations.csv",
            "text/csv",
            use_container_width=True,
        )
        second.download_button(
            "Download summary JSON",
            st.session_state.summary_json,
            f"{Path(st.session_state.source_name).stem}_summary.json",
            "application/json",
            use_container_width=True,
        )
        st.caption(
            "The summary records source characteristics and result totals. A production deployment should "
            "also record model checksum, dataset version, deployment metadata and reviewer decisions."
        )

st.markdown(
    """<div class="footer"><span>AquaTrack · Applied computer vision for marine observation</span>
    <span>Transparent outputs · Reproducible analysis · Human review</span></div>""",
    unsafe_allow_html=True,
)
