AquaTrack+ project folder
=========================

Open the .aqp file in AquaTrack+ to work on this project.

media/
  videos/       Video files copied into the project.
  sequences/    One folder per sequence. A sequence of videos holds
                sequence.json (its clips, in play order) and a README.txt
                saying how to open it; its videos stay in media/videos/.
                A BrackishMOT image sequence holds seqinfo.ini, img1/ and
                gt/gt.txt.
annotations/    Your reviewed edits (observations.json), imported ground
                truth, and detector output.
models/         Detector weights with their model.json manifests. The
                Models page lists whatever is here.
exports/        CSV and MOTChallenge exports.
snapshots/      Frame snapshots saved from Review.

Add, rename and reorder media from inside AquaTrack+ (Dashboard or Videos
page) rather than in this folder: annotations are stored by media name.

Paths inside this folder are stored relative to it, so the whole folder can be
moved, copied or zipped and still open. Media linked from elsewhere is stored
as an absolute path and must stay where it is.
