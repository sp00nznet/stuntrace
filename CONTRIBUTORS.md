# Contributors

Maintained by [@sp00nznet](https://github.com/sp00nznet).

## Issue reports and testing

### @Bluekey007

Reported [#1](https://github.com/sp00nznet/stuntrace/issues/1): a fresh clone could
not build. The snesrecomp submodule pinned a LakeSnes commit
(`angelo-wf/LakeSnes@47bffca`) that no longer existed upstream, so the clone came
back without the GSU-2 sources (`snes/gsu.c`, `snes/gsu.h`) and the Super FX
build failed. The report named the exact commits and URLs, which made it quick to
confirm the pins now resolve through the sp00nznet LakeSnes fork.
