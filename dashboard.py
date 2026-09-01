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
    page_title="AquaTrack Mission Console",
    page_icon="◉",
    layout="wide",
    initial_sidebar_state="expanded",
)


STYLES = """
<style>
:root {
  --navy-950:#061923; --navy-900:#092531; --navy-800:#103642; --navy-700:#174b58;
  --cyan:#38c6d3; --cyan-soft:#dff6f8; --orange:#f0a23b; --red:#d95c5c;
  --green:#2ba879; --paper:#f3f6f7; --white:#fff; --ink:#102832; --muted:#60747c;
  --line:#d5dfe2;
}
html, body, [class*="css"] { font-family: Inter, "Segoe UI", Arial, sans-serif; }
.stApp { background: var(--paper); color:var(--ink); }
[data-testid="stHeader"] { background:transparent; }
[data-testid="stSidebar"] { background:var(--navy-950); border-right:1px solid #173a46; }
[data-testid="stSidebar"] * { color:#e7f1f3; }
[data-testid="stSidebar"] hr { border-color:#24434d; }
.block-container { max-width:1680px; padding:1rem 1.6rem 3rem; }
h1,h2,h3 { letter-spacing:-.02em; }

.mission-header { background:var(--navy-950); color:white; border-top:3px solid var(--cyan);
  padding:1.15rem 1.4rem; display:flex; align-items:center; justify-content:space-between;
  gap:1rem; margin-bottom:1rem; box-shadow:0 5px 18px rgba(4,26,36,.14); }
.mission-brand { display:flex; align-items:center; gap:.85rem; }
.mission-mark { width:38px; height:38px; border:1px solid #4b8590; border-radius:50%;
  display:grid; place-items:center; color:var(--cyan); font-weight:800; }
.mission-name { font-weight:750; letter-spacing:.08em; font-size:.94rem; }
.mission-subtitle { color:#8fb1b8; font-size:.7rem; letter-spacing:.09em; margin-top:.2rem; }
.header-status { display:flex; flex-wrap:wrap; justify-content:flex-end; gap:.55rem; }
.status { padding:.36rem .6rem; border:1px solid #31545f; background:#0b2b37;
  font-size:.69rem; letter-spacing:.06em; white-space:nowrap; }
.status b { color:#a6c5ca; margin-right:.35rem; }
.status-ready { color:#68dfb2; } .status-caution { color:#ffc670; }

.section-head { display:flex; justify-content:space-between; align-items:flex-end; gap:1rem;
  margin:1.15rem 0 .65rem; }
.section-kicker { color:#267482; font-size:.67rem; font-weight:750; letter-spacing:.11em; text-transform:uppercase; }
.section-title { color:var(--ink); font-size:1.18rem; font-weight:750; margin-top:.15rem; }
.section-note { color:var(--muted); font-size:.78rem; }

.panel { background:var(--white); border:1px solid var(--line); padding:1rem 1.1rem;
  box-shadow:0 2px 8px rgba(19,53,64,.035); height:100%; }
.panel-dark { background:var(--navy-900); border:1px solid #264b57; color:#e8f2f3; padding:1rem 1.1rem; }
.panel-title { font-weight:750; font-size:.86rem; margin-bottom:.55rem; }
.panel-copy { color:var(--muted); font-size:.78rem; line-height:1.55; }
.panel-dark .panel-copy { color:#99b6bc; }
.mono { font-family:"Cascadia Mono",Consolas,monospace; font-size:.72rem; }

.readiness-row { display:flex; justify-content:space-between; align-items:center;
  border-bottom:1px solid #e2e9eb; padding:.46rem 0; font-size:.76rem; }
.readiness-row:last-child { border-bottom:0; }
.check { color:var(--green); font-weight:800; } .pending { color:var(--orange); font-weight:800; }
.qc-banner { border-left:4px solid var(--orange); background:#fff8e9; padding:.75rem .9rem;
  color:#674f20; font-size:.77rem; line-height:1.5; }
.qc-pass { border-left-color:var(--green); background:#edf9f4; color:#225d49; }

[data-testid="stMetric"] { background:white; border:1px solid var(--line); padding:.8rem .9rem; }
[data-testid="stMetricLabel"] { color:var(--muted); font-size:.73rem; }
[data-testid="stMetricValue"] { color:var(--navy-900); font-weight:750; }
[data-testid="stFileUploader"] { background:white; border:1px dashed #7ca9b1; padding:.45rem; }
[data-testid="stFileUploader"] section { background:#f7fafb; border:0; border-radius:0; }
.stButton>button { border-radius:2px; min-height:2.75rem; font-weight:700; border:1px solid #0d6877;
  background:#0c7180; color:white; box-shadow:none; }
.stButton>button:hover { background:#095b68; color:white; border-color:#095b68; }
[data-testid="stTabs"] button { font-weight:700; font-size:.79rem; }
[data-testid="stDataFrame"] { border:1px solid var(--line); }
.provenance { background:#071e29; color:#b8d3d8; padding:.9rem; border-left:3px solid var(--cyan);
  font-family:"Cascadia Mono",Consolas,monospace; font-size:.7rem; line-height:1.55; overflow-wrap:anywhere; }
.footer { margin-top:2rem; padding:.8rem 0; border-top:1px solid var(--line); display:flex;
  justify-content:space-between; color:#70858c; font-size:.68rem; letter-spacing:.03em; }

/* Minimal product UI: scientific rigor without a mission-control aesthetic. */
html, body, [class*="css"] { font-family:Inter, ui-sans-serif, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
.stApp { background:#ffffff; color:#202123; }
[data-testid="stSidebar"] { background:#f7f7f5; border-right:1px solid #e6e6e3; }
[data-testid="stSidebar"] * { color:#30302e; }
[data-testid="stSidebar"] hr { border-color:#e1e1dd; }
.block-container { max-width:1400px; padding:1.4rem 2rem 4rem; }
.mission-header { background:#ffffff; color:#202123; border:0; border-bottom:1px solid #e5e5e2;
  padding:.75rem 0 1rem; box-shadow:none; margin-bottom:1.8rem; }
.mission-mark { width:34px; height:34px; border:0; background:#202123; color:#fff; font-size:.72rem; }
.mission-name { letter-spacing:-.01em; font-size:.95rem; }
.mission-subtitle { color:#777771; letter-spacing:.025em; text-transform:none; font-size:.72rem; }
.header-status { gap:.4rem; }
.status { border:1px solid #e1e1dd; border-radius:999px; background:#fafaf9; color:#565653;
  padding:.34rem .62rem; letter-spacing:0; font-size:.69rem; }
.status b { color:#92928c; }
.status-ready { color:#16794f; background:#f0f8f4; border-color:#d5eadf; }
.status-caution { color:#8b6119; background:#fff9ed; border-color:#f0e3c8; }
.section-head { margin:1.7rem 0 .75rem; }
.section-kicker { color:#898984; letter-spacing:.055em; }
.section-title { color:#202123; font-size:1.25rem; }
.section-note { color:#777771; }
.panel { border:1px solid #e5e5e2; border-radius:12px; padding:1.1rem 1.2rem; box-shadow:none; }
.panel-title { color:#2b2b29; }
.panel-copy { color:#6c6c67; }
.readiness-row { border-color:#ececea; }
[data-testid="stMetric"] { border:1px solid #e5e5e2; border-radius:12px; padding:.9rem 1rem; box-shadow:none; }
[data-testid="stMetricValue"] { color:#202123; }
[data-testid="stFileUploader"] { border:1px dashed #c6c6c1; border-radius:12px; padding:.55rem; }
[data-testid="stFileUploader"] section { background:#fafaf9; border-radius:9px; }
.stButton>button { border:1px solid #202123; border-radius:9px; background:#202123; color:white; min-height:2.8rem; }
.stButton>button:hover { background:#000; border-color:#000; }
[data-testid="stDataFrame"] { border:1px solid #e5e5e2; border-radius:10px; overflow:hidden; }
.qc-banner { border-left:3px solid #d89a35; border-radius:8px; background:#fffaf1; }
.qc-pass { border-left-color:#268b62; background:#f1faf6; }
.provenance { background:#f7f7f5; color:#3f3f3b; border:1px solid #e5e5e2; border-left:3px solid #202123;
  border-radius:8px; }
.footer { color:#8a8a84; font-size:.69rem; letter-spacing:0; }
</style>
"""
st.markdown(STYLES, unsafe_allow_html=True)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def initialize_state() -> None:
    defaults = {
        "analysis": None,
        "annotated_video": None,
        "observations_csv": None,
        "summary_json": None,
        "manifest_json": None,
        "source_name": None,
        "run_id": None,
        "review_records": [],
    }
    for key, value in defaults.items():
        st.session_state.setdefault(key, value)


def clear_analysis() -> None:
    for key in (
        "analysis", "annotated_video", "observations_csv", "summary_json",
        "manifest_json", "source_name", "run_id",
    ):
        st.session_state[key] = None
    st.session_state.review_records = []


def format_duration(seconds: float) -> str:
    minutes, remainder = divmod(int(seconds), 60)
    return f"{minutes:02d}:{remainder:02d}"


initialize_state()

with st.sidebar:
    st.markdown("### ◉ AQUATRACK")
    st.caption("Marine video analysis")
    st.divider()
    st.caption("DEPLOYMENT METADATA")
    project_id = st.text_input("Project ID", "AQ-2026-001")
    deployment_id = st.text_input("Deployment ID", "DEP-001")
    station_id = st.text_input("Station", "STN-001")
    operator = st.text_input("Operator", "")
    camera_id = st.text_input("Camera system", "CAM-01")
    meta_left, meta_right = st.columns(2)
    latitude = meta_left.number_input("Latitude", value=47.5700, format="%.5f")
    longitude = meta_right.number_input("Longitude", value=-52.7000, format="%.5f")
    depth_m = st.number_input("Depth (m)", min_value=0.0, value=0.0, step=0.5)
    visibility = st.selectbox("Visibility", ["Not assessed", "Good", "Moderate", "Poor"])

    st.divider()
    st.caption("MODEL & INFERENCE")
    weights = st.text_input("Model weights", "yolo11n.pt")
    confidence = st.slider("Detection confidence", 0.05, 0.95, 0.30, 0.05)
    with st.expander("Tracking parameters"):
        match_iou = st.slider("Association IoU", 0.05, 0.90, 0.30, 0.05)
        confirmation_hits = st.slider("Confirmation frames", 1, 10, 3)
        missed_frames = st.slider("Occlusion tolerance", 1, 60, 20)
    with st.expander("Counting parameters"):
        line_position = st.slider("Counting line", 0.10, 0.90, 0.50, 0.05)
        direction = st.selectbox("Direction", ["any", "up", "down"])
    st.divider()
    if weights == "yolo11n.pt":
        st.warning("BASELINE MODEL\n\nGeneric COCO weights are not validated for scientific reporting.")
    else:
        st.success(f"CUSTOM MODEL\n\n{Path(weights).name}")

run_state = "COMPLETE" if st.session_state.analysis else "STANDBY"
run_class = "status-ready" if st.session_state.analysis else "status-caution"
st.markdown(
    f"""
    <header class="mission-header">
      <div class="mission-brand"><div class="mission-mark">AQ</div><div>
        <div class="mission-name">AquaTrack</div>
        <div class="mission-subtitle">Underwater video observation and review</div>
      </div></div>
      <div class="header-status">
        <span class="status"><b>PROJECT</b>{project_id}</span>
        <span class="status"><b>STATION</b>{station_id}</span>
        <span class="status"><b>TIME</b>{utc_now()}</span>
        <span class="status {run_class}"><b>STATE</b>{run_state}</span>
      </div>
    </header>
    """,
    unsafe_allow_html=True,
)

st.markdown(
    '<div class="section-head"><div><div class="section-kicker">01 / Data intake</div>'
    '<div class="section-title">Source video and deployment readiness</div></div>'
    '<div class="section-note">Preserve original media and record collection context before inference.</div></div>',
    unsafe_allow_html=True,
)
upload_col, readiness_col = st.columns([1.7, 1], gap="medium")
with upload_col:
    uploaded = st.file_uploader(
        "Source survey video",
        type=["mp4", "mov", "avi", "mkv"],
        help="Accepted containers: MP4, MOV, AVI and MKV.",
        on_change=clear_analysis,
    )
    if uploaded:
        size_mb = uploaded.size / (1024 * 1024)
        st.caption(f"SOURCE  {uploaded.name}  ·  {size_mb:.1f} MB  ·  SHA-256 calculated at run time")
    analyze = st.button(
        "EXECUTE ANALYSIS RUN",
        type="primary",
        use_container_width=True,
        disabled=uploaded is None,
    )
with readiness_col:
    metadata_ready = bool(project_id and deployment_id and station_id and camera_id)
    st.markdown(
        f"""<div class="panel"><div class="panel-title">PRE-RUN READINESS</div>
        <div class="readiness-row"><span>Source media</span><span class="{'check' if uploaded else 'pending'}">{'READY' if uploaded else 'PENDING'}</span></div>
        <div class="readiness-row"><span>Deployment identifiers</span><span class="{'check' if metadata_ready else 'pending'}">{'READY' if metadata_ready else 'INCOMPLETE'}</span></div>
        <div class="readiness-row"><span>Spatial position</span><span class="check">RECORDED</span></div>
        <div class="readiness-row"><span>Visibility assessment</span><span class="{'check' if visibility != 'Not assessed' else 'pending'}">{visibility.upper()}</span></div>
        <div class="readiness-row"><span>Model validation</span><span class="{'pending' if weights == 'yolo11n.pt' else 'check'}">{'BASELINE ONLY' if weights == 'yolo11n.pt' else 'CUSTOM'}</span></div>
        </div>""",
        unsafe_allow_html=True,
    )

if analyze and uploaded is not None:
    try:
        source_bytes = uploaded.getvalue()
        run_id = f"RUN-{uuid.uuid4().hex[:10].upper()}"
        started_utc = utc_now()
        with tempfile.TemporaryDirectory(prefix="aquatrack-") as temporary:
            work = Path(temporary)
            source = work / uploaded.name
            source.write_bytes(source_bytes)
            config = load_config("configs/default.yaml")
            config.model.weights = weights
            config.model.confidence = confidence
            config.tracking.match_iou = match_iou
            config.tracking.min_confirmed_hits = confirmation_hits
            config.tracking.max_missed_frames = missed_frames
            config.counting.line_position = line_position
            config.counting.direction = direction

            progress = st.progress(0, text=f"{run_id} · validating source")
            progress.progress(15, text=f"{run_id} · loading inference model")
            detector = YoloDetector(config.model)
            progress.progress(30, text=f"{run_id} · decoding and tracking observations")
            result = process_video(source, work / "outputs", detector, config)
            progress.progress(90, text=f"{run_id} · writing auditable products")
            summary = json.loads(result.summary_path.read_text(encoding="utf-8"))
            manifest = {
                "schema": "aquatrack.run-manifest.v1",
                "run_id": run_id,
                "started_utc": started_utc,
                "completed_utc": utc_now(),
                "project": {"project_id": project_id, "deployment_id": deployment_id, "station_id": station_id},
                "platform": {
                    "camera_id": camera_id, "operator": operator or None,
                    "latitude": latitude, "longitude": longitude, "depth_m": depth_m,
                    "visibility": visibility,
                },
                "source": {"filename": uploaded.name, "size_bytes": len(source_bytes), "sha256": sha256_bytes(source_bytes)},
                "model": {
                    "weights": weights, "validation_state": "baseline" if weights == "yolo11n.pt" else "custom-unverified",
                    "confidence_threshold": confidence,
                },
                "tracking": {
                    "method": "greedy_iou_baseline", "match_iou": match_iou,
                    "confirmation_frames": confirmation_hits, "max_missed_frames": missed_frames,
                },
                "counting": {"line_position": line_position, "direction": direction},
                "quality_control": {
                    "status": "SUSPECT" if weights == "yolo11n.pt" or visibility in {"Poor", "Not assessed"} else "NOT_REVIEWED",
                    "flags": [
                        flag for flag, condition in (
                            ("SUSPECT_GENERIC_MODEL", weights == "yolo11n.pt"),
                            ("SUSPECT_LOW_VISIBILITY", visibility == "Poor"),
                            ("NOT_EVALUATED_VISIBILITY", visibility == "Not assessed"),
                            ("NOT_HUMAN_REVIEWED", True),
                        ) if condition
                    ],
                },
                "results": summary,
            }
            st.session_state.analysis = summary
            st.session_state.annotated_video = result.video_path.read_bytes()
            st.session_state.observations_csv = result.csv_path.read_bytes()
            st.session_state.summary_json = result.summary_path.read_bytes()
            st.session_state.manifest_json = json.dumps(manifest, indent=2).encode("utf-8")
            st.session_state.source_name = uploaded.name
            st.session_state.run_id = run_id
            st.session_state.review_records = []
            progress.progress(100, text=f"{run_id} · complete")
    except Exception as exc:
        st.error(f"Analysis run failed: {exc}")

summary = st.session_state.analysis
if not summary:
    st.markdown(
        '<div class="section-head"><div><div class="section-kicker">System state</div>'
        '<div class="section-title">Awaiting source data</div></div></div>',
        unsafe_allow_html=True,
    )
    empty_cols = st.columns(3)
    empty_content = [
        ("OBSERVATION", "Detections remain linked to frame, timestamp and source evidence."),
        ("QUALITY", "Automated flags identify missing context, low visibility and unvalidated models."),
        ("PROVENANCE", "Every run receives a stable identifier, UTC timestamps and source checksum."),
    ]
    for column, (title, copy) in zip(empty_cols, empty_content):
        column.markdown(
            f'<div class="panel"><div class="panel-title">{title}</div><div class="panel-copy">{copy}</div></div>',
            unsafe_allow_html=True,
        )
else:
    manifest = json.loads(st.session_state.manifest_json)
    observations = pd.read_csv(io.BytesIO(st.session_state.observations_csv))
    qc_status = manifest["quality_control"]["status"]
    st.markdown(
        f'<div class="section-head"><div><div class="section-kicker">02 / Analysis results</div>'
        f'<div class="section-title">{st.session_state.run_id} · Operational review</div></div>'
        f'<div class="section-note">QC STATE: {qc_status}</div></div>',
        unsafe_allow_html=True,
    )
    metric_cols = st.columns(6)
    metric_cols[0].metric("Confirmed tracks", summary["unique_confirmed_tracks"])
    metric_cols[1].metric("Line crossings", summary["line_crossing_total"])
    metric_cols[2].metric("Frames", f'{summary["frames_processed"]:,}')
    metric_cols[3].metric("Duration", format_duration(summary["duration_seconds"]))
    metric_cols[4].metric("Source FPS", f'{summary["source_fps"]:.1f}')
    metric_cols[5].metric("QC state", qc_status)

    review_tab, events_tab, qc_tab, provenance_tab, export_tab = st.tabs(
        ["REVIEW WORKSPACE", "EVENT DATA", "QA / QC", "PROVENANCE", "EXPORT"]
    )
    with review_tab:
        video_col, decision_col = st.columns([2.25, 1], gap="medium")
        with video_col:
            st.video(st.session_state.annotated_video)
            st.caption("GREEN: active confirmed track · AMBER: track crossed counting line · CYAN: counting boundary")
        with decision_col:
            st.markdown('<div class="panel-title">HUMAN REVIEW RECORD</div>', unsafe_allow_html=True)
            review_state = st.selectbox("Decision", ["Not reviewed", "Accepted", "Accepted with changes", "Rejected"])
            reviewer = st.text_input("Reviewer", operator, key="reviewer_name")
            review_notes = st.text_area("Review notes", height=120, placeholder="Record uncertainty, false detections or corrections…")
            if st.button("RECORD REVIEW DECISION", use_container_width=True):
                if review_state == "Not reviewed":
                    st.warning("Select a completed review decision.")
                else:
                    record = {
                        "run_id": st.session_state.run_id,
                        "reviewed_utc": utc_now(),
                        "reviewer": reviewer or "unspecified",
                        "decision": review_state,
                        "notes": review_notes,
                    }
                    st.session_state.review_records.append(record)
                    st.success("Review decision recorded for this session.")
            if st.session_state.review_records:
                st.dataframe(pd.DataFrame(st.session_state.review_records), hide_index=True, use_container_width=True)

        st.markdown("#### Temporal observation density")
        if not observations.empty:
            timeline = observations.assign(second=observations["timestamp_seconds"].astype(int)).groupby("second").size()
            st.line_chart(timeline, color="#168697", height=180)
        else:
            st.info("No confirmed observations are available for temporal review.")

    with events_tab:
        filter_a, filter_b, filter_c = st.columns(3)
        classes = sorted(observations["class_name"].dropna().unique().tolist()) if not observations.empty else []
        selected_classes = filter_a.multiselect("Class", classes, default=classes)
        minimum_confidence = filter_b.slider("Minimum confidence", 0.0, 1.0, 0.0, 0.05)
        counted_only = filter_c.checkbox("Counted tracks only")
        filtered = observations.copy()
        if selected_classes:
            filtered = filtered[filtered["class_name"].isin(selected_classes)]
        elif classes:
            filtered = filtered.iloc[0:0]
        filtered = filtered[filtered["confidence"] >= minimum_confidence]
        if counted_only:
            filtered = filtered[filtered["counted"]]
        st.dataframe(
            filtered,
            use_container_width=True,
            hide_index=True,
            column_config={
                "confidence": st.column_config.ProgressColumn("Confidence", min_value=0.0, max_value=1.0),
                "timestamp_seconds": st.column_config.NumberColumn("UTC-relative time (s)", format="%.2f"),
                "track_id": st.column_config.NumberColumn("Track ID", format="%d"),
            },
        )
        st.caption(f"DISPLAYING {len(filtered):,} OF {len(observations):,} OBSERVATION ROWS")

    with qc_tab:
        flags = manifest["quality_control"]["flags"]
        banner_class = "qc-banner qc-pass" if not flags else "qc-banner"
        st.markdown(
            f'<div class="{banner_class}"><strong>{qc_status}</strong><br>'
            f'{"No automated flags were raised." if not flags else " · ".join(flags)}</div>',
            unsafe_allow_html=True,
        )
        st.write("")
        qc_rows = [
            {"Check": "Source readability", "State": "PASS", "Evidence": f'{summary["frames_processed"]} decoded frames'},
            {"Check": "Deployment metadata", "State": "PASS" if metadata_ready else "FAIL", "Evidence": deployment_id},
            {"Check": "Visibility assessment", "State": "PASS" if visibility in {"Good", "Moderate"} else "SUSPECT", "Evidence": visibility},
            {"Check": "Model suitability", "State": "SUSPECT" if weights == "yolo11n.pt" else "NOT VERIFIED", "Evidence": Path(weights).name},
            {"Check": "Human review", "State": "PASS" if st.session_state.review_records else "PENDING", "Evidence": f'{len(st.session_state.review_records)} records'},
        ]
        st.dataframe(pd.DataFrame(qc_rows), hide_index=True, use_container_width=True)
        st.caption("Automated QC flags guide review; they do not replace scientific validation.")

    with provenance_tab:
        st.markdown("#### Run manifest")
        st.markdown(
            f'<div class="provenance">RUN ID&nbsp;&nbsp;&nbsp;&nbsp;{manifest["run_id"]}<br>'
            f'SOURCE SHA&nbsp;&nbsp;{manifest["source"]["sha256"]}<br>'
            f'START UTC&nbsp;&nbsp;&nbsp;{manifest["started_utc"]}<br>'
            f'END UTC&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{manifest["completed_utc"]}<br>'
            f'MODEL&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;{manifest["model"]["weights"]}<br>'
            f'LOCATION&nbsp;&nbsp;&nbsp;&nbsp;{latitude:.5f}, {longitude:.5f}</div>',
            unsafe_allow_html=True,
        )
        st.json(manifest, expanded=False)

    with export_tab:
        st.markdown("#### Open data products")
        st.write("Export results with machine-readable provenance for downstream GIS, database or review workflows.")
        export_cols = st.columns(3)
        stem = Path(st.session_state.source_name).stem
        export_cols[0].download_button(
            "OBSERVATIONS CSV", st.session_state.observations_csv,
            f"{stem}_observations.csv", "text/csv", use_container_width=True,
        )
        export_cols[1].download_button(
            "SUMMARY JSON", st.session_state.summary_json,
            f"{stem}_summary.json", "application/json", use_container_width=True,
        )
        export_cols[2].download_button(
            "RUN MANIFEST JSON", st.session_state.manifest_json,
            f"{stem}_run_manifest.json", "application/json", use_container_width=True,
        )
        if st.session_state.review_records:
            review_bytes = json.dumps(st.session_state.review_records, indent=2).encode("utf-8")
            st.download_button("REVIEW RECORDS JSON", review_bytes, f"{stem}_reviews.json", "application/json")

st.markdown(
    """<div class="footer"><span>AquaTrack · Research build 0.1.0</span>
    <span>Observations are not abundance estimates · Retain source evidence · Verify model outputs</span></div>""",
    unsafe_allow_html=True,
)
