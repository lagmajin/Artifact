# Artifact CLI interactive shell

Artifact can run a project without opening the GUI and expose the same command
surface interactively or from a command file.

```text
Artifact.exe --interactive project.artifact
Artifact.exe --script build_preview.artifact-cli
```

The top-level render namespace is reserved for the headless executor:

```text
Artifact.exe render project.artifact --output preview.mp4 --start 0 --end 120
```

Its arguments are parsed and validated independently from GUI launch options.

## Typical session

```text
project.stats
composition.list
composition.select MainComp
layer.list
layer.select Background
property.get opacity
property.get opacity --json
property.set opacity 0.5 --json
undo
redo
project.validate
project.save
project.open OtherProject.artifact
ls
select Main
get opacity --json
render.plan --json
render.list --json
render.enqueue render-job.json --start 0 --end 120 --output preview.mp4 --format mp4
render.status render-job.json --json
render.start render-job.json
render.finish render-job.json
quit
```

`source path.cli` executes another command file. Lines beginning with `#` are
ignored, and recursive `source` loops are rejected. `complete prefix` lists
registered commands for future tab-completion clients.

`--script` returns exit code `1` when a command is unknown and `2` when the
script file cannot be opened or the command-line argument is incomplete.

`render.plan` reports frame ranges, resolution, and frame rate. Add `--json`
for machine-readable output. `render.enqueue` writes an `artifact.render` job
manifest that can be consumed by a render worker or the render queue adapter.
Supported render formats are `png`, `jpg`, `jpeg`, `mp4`, `mov`, and `webm`.

`project.open` starts a nested session for another project; exiting that
session returns to the parent shell.
