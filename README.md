# ldn_mitm

A mitm kip modified from fs_mitm.

```
git submodule update --init --recursive
```

# Compiling tip

Now there is a bug in `libstratosphere` causing the game crashing. (the PR is not merged. https://github.com/Atmosphere-NX/libstratosphere/pull/1). So before compile this kip, you need to patch `libstratosphere`.

```
git apply patch/00_sz.patch
```
