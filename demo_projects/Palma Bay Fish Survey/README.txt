AquaTrack+ project folder
=========================

Open the .aqp file in AquaTrack+ to work on this project.

media/        Sequences and video files copied into the project. A BrackishMOT
              sequence is a folder holding seqinfo.ini, img1/ and gt/gt.txt.
annotations/  Your reviewed edits (observations.json) and MOTChallenge gt.txt
              exports.
models/       Detector weights with their model.json manifests. The Models page
              lists whatever is here.
exports/      CSV and other exports.
snapshots/    Frame snapshots saved from Review.

Paths inside this folder are stored relative to it, so the whole folder can be
moved, copied or zipped and still open. Media linked from elsewhere is stored
as an absolute path and must stay where it is.
