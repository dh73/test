# Sanity Checks for Formal Property Verification
## How to run
Each task can be called independently, for example:
```bash
sby -f fifo.sby bug2w
```
* Tasks that does not detect any issue: prove, bug1 and bug2.
* Tasks that detects the errors: bug1w and bug2w.

See the defines in the `fifo.sv`.
