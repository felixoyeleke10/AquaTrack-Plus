# Palma Bay Fish Survey: what is in this demo

Everything here is real data or real model output. Nothing is synthetic.

## Footage

SFISHTRACK dataset clips: GoPro Hero 3, about 10 m depth, Palma's bay,
recorded 22 May 2021 (from the dataset's metadata files). The videos are
copied into `media/videos/` and played directly from the MP4 files.

| Media | Frames | Boxes come from |
|---|---|---|
| video_019.mp4 (dense school of fish) | 1,067 | RT-DETR-L detections + ByteTrack (unverified) |
| video_045 to video_048.mp4 | 683 / 589 / 283 / 709 | SFISHTRACK ground truth (verified) |
| Transect 045-048 (video sequence of the four clips) | 2,264 | YOLO11n detections + ByteTrack (unverified) |
| brackishMOT-36 (image sequence) | 200 | BrackishMOT ground truth |

## Ground truth

`annotations/ground_truth/*.gt.txt` were produced by
`tools/sfishtrack_to_mot.py`. SFISHTRACK annotates resampled frame extractions,
so each annotated image was matched to its MP4 frame by pixels (median
difference about 0.35 grey levels on all four clips). Annotation frame k is
not MP4 frame k; for video_045 the annotations start 59 frames into the MP4.

## Models

Both were trained on SFISHTRACK in this repository (`runs/`):

- RT-DETR-L (rtdetr_l_converged): mAP50 0.34, recall 0.59, about 11 fps on this GPU.
- YOLO11n (sfishtrack_15ep): mAP50 0.31, about 26 fps end to end.

They are modest detectors. In video_019's dense school, ByteTrack fragments
many tracks (867 track IDs over 35 s), which the Review page's identity-switch
suggestions and Merge tracks are designed to correct. Say this plainly when
presenting: it is an honest baseline, not a tuned production model.

## Re-running

In Review: open a video and press **Detect & track** (choose a model when
asked), or **Import annotations** with a `gt.txt`. Or from a shell:

```powershell
build\aquatrack-cli.exe project analyze "demo_projects\Palma Bay Fish Survey\Palma Bay Fish Survey.aqp" "demo_projects\Palma Bay Fish Survey\media\videos\video_019.mp4" --model sfishtrack_15ep
```
