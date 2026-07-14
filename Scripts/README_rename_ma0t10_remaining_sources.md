# MA0T10 remaining source rename

The branch already contains `Scripts/rename_ma0t10_remaining_sources.ps1`.

Run from the repository root or from `Scripts`:

```powershell
powershell -ExecutionPolicy Bypass -File .\Scripts\rename_ma0t10_remaining_sources.ps1 -Apply

git status
git add -A
git commit -m "Rename remaining source folders to MA0T10"
git push
```

The script moves:

- `Source/ma0t10_dt/MA0T10` -> `Source/ma0t10_dt/MA0T10`
- remaining editor commandlet files under `Source/ma0t10_dtEditor/Private` -> `Source/ma0t10_dtEditor/Private`

It also replaces text references:

- `MA0T10` -> `MA0T10`
- `ma0t10_dtEditor` -> `ma0t10_dtEditor`
- `ma0t10_dt` -> `ma0t10_dt`
- `ma0t10` -> `ma0t10`
