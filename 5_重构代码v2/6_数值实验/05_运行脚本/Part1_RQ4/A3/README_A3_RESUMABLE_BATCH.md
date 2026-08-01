# A3 resumable formal batch

The controller runs exactly one scale-seed GA-BP task at a time, in frozen queue order. It marks a task `COMPLETED` only after required files and validation pass. `request_pause.sh BATCH_DIR` lets the current task finish, then pauses and releases the resource lock. `resume_a3.sh BATCH_DIR` resumes the same batch and skips completed tasks.

Generation-level checkpoint recovery is not supported by the existing GA executable. The recovery unit is one complete scale-seed GA run; an interrupted run is preserved and is not silently resumed from a fabricated generation.
