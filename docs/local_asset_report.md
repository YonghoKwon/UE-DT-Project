# Local Asset Report

`Scripts/report_local_project_status.ps1` is a local decision-aid for the Unreal
project copy, especially `C:\Unreal Projects\m7at10_dt`.

The script separates known local decision points from unexpected untracked files.
This keeps generated outputs, vendor/sample content, binary WBP edits, and local
settings out of code commits until each item has an explicit project decision.
The active decision list and completion evidence are tracked in
`docs/remaining_work.md`.

## Basic Usage

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1"
```

Useful machine-readable output:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -Json
```

## Gate Options

Fail when generated or local-output items are present:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnGeneratedOutput
```

Fail when specific decision categories are present:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```

Fail when Git sees an untracked file that is not covered by the known decision
point inventory:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnUnclassifiedUntracked
```

Fail when a known decision-point path has already been staged:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\report_local_project_status.ps1" -FailOnStagedDecisionPoints
```

## Current Categories

- `ReviewCandidate`: small project or binary edits that need manual review.
- `LargeContentCandidate`: Unreal content folders that may be valid production
  assets, but should be committed only after an explicit asset/vendor decision.
  The current large `Content/*` folders in the local project are confirmed
  unused local assets, so they are treated as keep-local cleanup candidates
  rather than repository-acceptance candidates.
- `SampleOrThirdParty`: sample or third-party content that should not enter the
  project accidentally.
- `GeneratedOutput`: packaged outputs such as `Windows/` and `Windows.zip`.
- `GeneratedOrLocalConfig`: local runtime configuration.

`Config/Game.ini` receives an additional detected note when it contains
`[DTCoreRuntimeOverride]`. If every override value is blank, treat it as a
local-only runtime override unless shared endpoint defaults are explicitly
required. The local asset report classifies that empty override shape as
`KeepLocal` and `KeepLocalByDecision`, so it should stay untracked rather than
waiting for repository acceptance. If any endpoint or credential value is
populated, review it for environment or secret leakage before staging.

Static runtime-config readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1"
```

This check passes when `Config/Game.ini` is absent or its
`[DTCoreRuntimeOverride]` values are empty, and fails if local endpoint or
credential values are present.

`Content/M7AT10/UI/WBP_VirtualSensorMonitor.uasset` receives an additional
detected note because it is a binary Designer widget. Keep it untracked until it
has been opened in Unreal Editor, optional bindings have been checked against
`docs/widget_designer_setup.md`, and a PIE smoke pass confirms that the widget
does not crash or show stale sensor status.
Use `Scripts/export_monitor_wbp_decision_report.ps1` for a focused WBP review
packet. It reuses `report_local_project_status.ps1`, reports the WBP
`ReviewQueue`, `CommitReadiness`, `EvidenceStatus`, `MissingEvidenceCount`, and
`ReadyToStage` state, and exports a manual acceptance checklist for editor open
verification, optional binding check, PIE smoke result, and production WBP
acceptance. `Scripts/report_precommit_summary.ps1` also surfaces the same WBP
Git/evidence state, missing acceptance items, setup-doc contract status,
preflight status, acceptance-evidence validation status, and manual editor
verification boundary before each commit. This report is review evidence, not
approval by itself.
Pass `-EvidencePath` to inspect a candidate
`LocalAssetDecisionEvidenceV1` file, and use `-FailOnIncompleteEvidence` in a
pre-commit gate when the binary WBP must not be staged until its evidence is
complete.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath ".\docs\local_asset_decisions.evidence.json" -FailOnIncompleteEvidence
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\prepare_monitor_wbp_editor_review.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_wbp_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath ".\docs\local_asset_decisions.evidence.json" -Json
```

`prepare_monitor_wbp_editor_review.ps1` is the last safe automation step before
manual Designer work. It creates a timestamped backup of
`WBP_VirtualSensorMonitor.uasset` under `Saved/Backups/MonitorWbp`, records the
pre-edit SHA256 hash, generates the monitor WBP acceptance package, and writes a
review checklist under `Saved/Reports/MonitorWbpEditorReview`. It writes only
under `Saved`, does not edit Unreal assets, and never stages the WBP.
The checklist now lists the required DisplayData rows, including
`LazExportText`, state helper checks for `IsShowingLidar`, `HasBoundCamera`,
and `HasBoundLidar`, manual export message evidence from
`GetLastManualExportMessage`, evidence-file completion, and post-edit strict
validation.
The monitor WBP acceptance template and validator derive optional widget names
from `VirtualSensorMonitorWidget.h` `BindWidgetOptional` properties rather than
from a separate hard-coded checklist. This prevents stale evidence names from
approving a WBP that would not auto-bind to the native widget.
The same acceptance evidence requires a `DisplayData visual match` section so
the editor reviewer maps `GetMonitorDisplayData()` rows to visible WBP
TextBlocks during PIE. This keeps the binary asset acceptance tied to the
current native monitor status contract instead of a screenshot-only review.
The required DisplayData rows include `LazExportText`, so the production WBP
must visibly preserve the LAZ placeholder/compressor/true-validation boundary
rather than only showing a generic export success message.
`DisplayDataScreenMatchEvidence` must also be accepted in the manual acceptance
sections before the WBP can be considered stageable.

Large content candidates and sample/third-party folders include extension counts
and the largest files in each folder. Use this to spot built-data-heavy map
packages, oversized textures, vendor packs, and copied sample projects before
deciding whether the repository should own them.
For the current local project state, `Content/ChemicalPlantEnv`,
`Content/Mega_Crane`, `Content/Materials`, `Content/Meshes`, and
`Content/Textures` are unused local asset folders. Do not stage them; either keep
them ignored locally or remove them manually after Unreal reference/dependency
checks. `Scripts/report_precommit_summary.ps1` now surfaces this split directly:
unused cleanup candidate count and size, the largest cleanup candidate, and the
remaining repository-acceptance candidate paths. Cleanup candidate means keep
ignored or manually remove after map/WBP dependency checks; it is not ready to
stage. The same pre-commit summary also surfaces sample/third-party candidate
count and size, currently `Samples/PixelStreaming`, with the boundary that copied
PixelStreaming samples are excluded from the current LiDAR/virtual-sensor scope
and must not count toward remaining implementation work. The local folder still
stays untracked so it cannot slip into a commit accidentally.
If PixelStreaming becomes a future project requirement, reopen ownership,
redistribution approval, and documentation-alternative decisions in that scope.

Every decision point also reports:

- `GitState`: `Untracked`, `Staged`, `TrackedModified`, or `CleanOrIgnored`.
- `CommitReadiness`: `BlockedByManualDecision`,
  `DoNotCommitGeneratedOutput`, or `NotPresent`.
- `ReviewQueue`: `ReadyToStage`, `NeedsOwnerDecision`, `KeepLocal`, or
  `NotPresent`.
- `DecisionOwner`: required decision authority such as
  `AssetOwnerRequired`, `ProjectOwnerRequired`, `ConfigOwnerRequired`,
  `PackagingOwnerRequired`, or `NotApplicable`.
- `DecisionStatus`: unresolved/accepted state such as `PendingOwnerDecision`,
  `EvidencePending`, `AcceptedForRepository`, `KeepLocal`,
  `DoNotCommitGeneratedOutput`, or `NotPresent`.
- `EvidenceNeeded`: concrete evidence that must be complete before a path can
  move to `ReadyToStage`.
- `EvidenceStatus`: current evidence review state such as `NoEvidenceRecord`,
  `PartialEvidence`, `AcceptedButEvidenceIncomplete`, `ReadyEvidenceAccepted`,
  or `GeneratedOutput`.
- `EvidenceSatisfied`: true only when required evidence is complete and the
  decision has reviewer/date acceptance metadata.
- `DecisionChecklist`: manual evidence that must be collected before staging.
- `ReviewPriority`: suggested owner-review order. Lower values should be
  reviewed first.
- `CommitBlocker`: true when a present decision point must not be staged yet.
- `BlockingReason`: human-readable reason why the path is not ready.
- `NextReviewAction`: the next concrete action needed from the owner or
  reviewer.

DecisionOwner does not mean ownership has been accepted. It is review-routing
metadata for who must make the decision.
EvidenceNeeded must be complete before ReadyToStage, and
`AcceptedForRepository` should only be used after the evidence is recorded.
ReadyToStage requires AcceptedForRepository. ReadyToStage requires complete evidence. AcceptedForRepository requires complete EvidenceNeeded.
PendingOwnerDecision remains NeedsOwnerDecision. EvidencePending remains NeedsOwnerDecision.
Recorded evidence must name reviewer, date, and source.
Generated output remains KeepLocal.

For unused local large content, the checklist asks for manual cleanup or
keep-local confirmation rather than repository acceptance. For any future large
content that is actually needed, the checklist still asks for asset source,
license, production dependency, size review, and storage/versioning strategy.
In short, needed large content still requires asset source, license, production
dependency, and storage evidence before repository acceptance.
For sample or third-party folders, it asks for project ownership, license/redistribution
terms, and whether documentation is preferable to committing copied sample
files. For the monitor WBP, it asks for editor open, binding verification, PIE
smoke evidence, and production-WBP acceptance. For `Config/Game.ini`, it asks
for endpoint/credential review and runtime-config policy validation.

Static large-content decision readiness:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1" -FailIfPresent
```

The first command summarizes local large-content and sample candidates. The
second command is a strict content-review gate and fails until ownership,
dependency, and size decisions are complete.

Export a review bundle:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1" -MarkdownPath ".\Saved\Reports\local_asset_decisions.md" -JsonPath ".\Saved\Reports\local_asset_decisions.json"
```

The exporter turns the same local asset report into a Markdown/JSON review
bundle. By default it only prints to the console; pass output paths when you
intentionally want generated review artifacts under `Saved/Reports`.

The Markdown export includes `Ready To Stage`, `Needs Owner Decision`, and
`Keep Local` review queues. Known local asset decision points should normally
start in `NeedsOwnerDecision`; generated package outputs should appear in
`KeepLocal`; no binary WBP, local config, large content, or sample folder should
move to `ReadyToStage` until the checklist evidence is complete.
The export also includes an `ActionPlan` sorted by `ReviewPriority` so WBP,
config, large content, sample folders, and generated outputs can be reviewed in
a stable order with an explicit `BlockingReason` and `NextReviewAction`.

Create an editable evidence template when a path is ready for owner review:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_evidence_template.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_local_asset_decision_report.ps1" -EvidencePath ".\docs\local_asset_decisions.evidence.json"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_local_asset_decision_evidence_workflow.ps1"
```

The template uses schema `LocalAssetDecisionEvidenceV1` and records
`DecisionEvidence` for each present non-generated decision point. A path moves
to `ReadyToStage` only when the evidence file sets
`DecisionStatus = AcceptedForRepository`, every `EvidenceNeeded` item has
`Status = Complete` and a non-empty `Source`, and `AcceptedBy`, `AcceptedAt`,
and `EvidenceSource` are filled in. Large content decisions require
owner/source/license, dependency, size, and storage/versioning evidence before
they can be accepted.
The generated template includes a `Summary` block with decision counts, pending
evidence item counts, and `TopBlockingPaths` so owner review can start from the
highest-priority local blockers without accepting any path implicitly.
`Scripts/validate_local_asset_decision_evidence_workflow.ps1` builds a
temporary git-backed project and verifies no-evidence, incomplete evidence,
blank reviewer/date/source, blank item source, pending decision, normalized path, `KeepLocal`,
duplicate normalized evidence path rejection, generated-output override,
complete accepted evidence, and Staged decision gate behavior. Duplicate normalized evidence paths are invalid because they can hide conflicting owner decisions.
The Staged decision gate must fail for blocked decision paths and pass for paths
that are `ReadyToStage` with complete accepted evidence.

## Recommended Workflow

1. Run the report before staging.
2. Resolve or explicitly accept each decision point.
3. Run with `-FailOnUnclassifiedUntracked` to catch newly created paths that the
   inventory does not know about yet.
4. Stage only intended source, docs, config, or content files.
5. Run with `-FailOnStagedDecisionPoints` before committing when local content
   decisions are still open.

## Readiness Wrapper

For regular local checks, use `Scripts/check_project_readiness.ps1`. It runs the
local asset report with `-FailOnUnclassifiedUntracked`, then runs the smoke test
script unless `-SkipSmoke` is passed.

For a fast commit-time local decision gate, use
`Scripts/invoke_local_decision_precommit_gate.ps1`. It is read-only and runs the
DTCore submodule guard, staged local-decision gate, sample staged-path gate,
large-content policy, runtime-config policy, monitor widget policy, WBP
preflight, WBP evidence report, and pre-commit summary. Default mode reports
incomplete WBP evidence without failing; pass
`-RequireAcceptedLocalDecisionEvidence` only when the local WBP evidence file is
expected to be complete.

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_local_decision_precommit_gate.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_local_decision_precommit_gate_policy.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1"
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipBuild
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -SkipLocalAssetDecisionEvidenceWorkflow
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SourceRepoRoot "." -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SkipSmoke
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SourceRepoRoot "." -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SkipSmoke -Json
```

Use `-SourceRepoRoot <source checkout> -ProjectRoot <local Unreal project>` when
the readiness wrapper is launched from the source checkout but the real
untracked asset/config decisions live in the local Unreal project. Policy and
documentation validators read the source checkout, while local asset, runtime
config, and large-content scans inspect the local project root.
When the pre-commit summary is run with `-IncludeReadiness`, treat
`ReadinessSummary.ReadinessMode = FastStaticPrecommit` as a quick gate only.
`FastReadinessPassed` does not mean full PIE, broker, LAZ, editor, or deployment
evidence is complete. Any skipped readiness steps are explicitly outside that
fast check and still need their manual/editor/deployment evidence before the
associated work can be called done.

Strict local-output gates can be enabled when a clean packaging state is needed:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnGeneratedOutput
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnStagedDecisionPoints
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnLargeContentCandidates
powershell -ExecutionPolicy Bypass -File ".\Scripts\check_project_readiness.ps1" -SkipSmoke -FailOnCategory LargeContentCandidate,SampleOrThirdParty
```

Runtime config decisions can be checked against a separate local Unreal project
root while keeping `ProjectRoot` pointed at the source repo docs and scripts:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_runtime_config_policy.ps1" -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_runtime_config_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_runtime_config_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -EvidencePath ".\docs\local_asset_decisions.evidence.json" -FailOnIncompleteEvidence
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_runtime_config_acceptance_template.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
```

The runtime config report emits `RecommendedDecision`. Empty
`[DTCoreRuntimeOverride]` values produce `KeepLocal` by default because blank
local override placeholders are not useful shared project defaults unless a
config owner explicitly accepts them.
It also reuses `report_local_project_status.ps1` to expose `ReviewQueue`,
`CommitReadiness`, `EvidenceStatus`, `MissingEvidenceCount`, and `ReadyToStage`
for `Config/Game.ini`. Use `-EvidencePath` to inspect a candidate
`LocalAssetDecisionEvidenceV1` record and `-FailOnIncompleteEvidence` when a
pre-commit gate should reject staging the local config until owner evidence is
complete.
The runtime config acceptance template is also read-only. It records key names,
counts, hash, secret-scan/log evidence fields, and owner decision placeholders
with `ValuesRedacted=true`, `ModifiesConfig=false`, and `StagesConfig=false`.
Do not store endpoint, credential, token, password, or secret values in the
evidence record.

Monitor WBP decisions can be inspected without mutating the binary asset:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_template.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_preflight_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_monitor_wbp_acceptance_package.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "."
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_monitor_wbp_acceptance_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -SourceRepoRoot "." -Json
```

The WBP decision report records binary metadata, Git state, setup-document
contract terms, `RecommendedDecision`, and an evidence draft. It cannot replace
Unreal Editor verification; it keeps editor-open, optional binding, PIE smoke,
and production acceptance evidence separate from file metadata.
The WBP acceptance template turns those manual gates into a fillable read-only
evidence structure with `EvidenceRunId`, operator, map/PIE session, log and
screenshot paths, optional binding rows, exported payload evidence, owner
acceptance fields, and the current asset hash. It does not modify or stage the
binary WBP.
The WBP preflight report is also read-only. It checks the current WBP hash, Git
state, setup-document contract, acceptance-template availability, missing
evidence count, and post-archive context before manual Editor/PIE review starts.
Preflight readiness is not WBP acceptance and does not permit staging the binary
asset.
The preflight report also records the edit boundary: direct binary patching is
not supported, Editor-mediated asset edits are required, and Codex should prefer
native C++/Blueprint-callable binding changes before any Designer asset edit.
If the WBP asset is edited, record pre-edit hash/backup, compile/save evidence,
post-edit hash, PIE smoke, and owner acceptance before staging.
The WBP acceptance package exporter writes a local
`Saved/Reports/MonitorWbpAcceptance` bundle with the preflight report, decision
report, fillable evidence JSON/Markdown, validation JSON, manual steps, and
strict follow-up commands. The package is local review evidence only; it does
not modify assets, stage files, or accept the binary WBP.
The WBP acceptance evidence validator is the stricter read-only gate for a
filled evidence file. It checks `AssetHashAlgorithm = SHA256`, the current WBP
hash, editor-open/compile evidence, optional binding crash-safety rows, PIE
smoke evidence, and production owner acceptance. Missing or incomplete evidence
is reported as incomplete; it throws only when `-FailOnIncompleteEvidence` is
explicitly requested.

Large content decisions can be summarized separately from generated package
outputs:

```powershell
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_large_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_large_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_large_content_cleanup_plan.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\invoke_unused_content_archive.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_unused_content_archive_evidence.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
powershell -ExecutionPolicy Bypass -File ".\Scripts\export_sample_content_decision_report.ps1" -ProjectRoot "C:\Unreal Projects\m7at10_dt"
powershell -ExecutionPolicy Bypass -File ".\Scripts\validate_large_content_decision_policy.ps1" -ProjectRoot "." -LocalProjectRoot "C:\Unreal Projects\m7at10_dt" -Json
```

The large content report reuses `report_local_project_status.ps1` and focuses on
`LargeContentCandidate` plus `SampleOrThirdParty` paths. It records size,
extension counts, largest files, risk, `RecommendedDecision`, and evidence
drafts. Content over 1 GB and copied samples default to `KeepLocal` until
source/license/dependency/storage evidence is complete and accepted.
The same report now exposes `BuiltDataHeavy`, `LargestFileRisk`,
`StorageRiskReason`, `RedistributionReviewRequired`, and `SampleRiskReason` so
map build data, single-file storage risk, and copied sample redistribution
questions are visible before repository ownership is accepted.
Use `validate_large_content_decision_policy.ps1 -ProjectRoot <source repo>
-LocalProjectRoot <local Unreal project>` when the policy/docs live in the
source checkout but the untracked content lives in the local Unreal project.
The cleanup plan is intentionally read-only: `DryRunOnly=true`,
`DeletesFiles=false`, and `ModifiesAssets=false`. It does not fix redirectors,
delete files, or modify Unreal assets. Each unused cleanup candidate remains
`ManualDeletionOnly=true` and `SafeToDelete=false` until map, WBP/widget, asset
registry/reference viewer, redirector, config/startup, post-move editor smoke,
and staging checks are recorded outside the script.
`invoke_unused_content_archive.ps1` is the separate local execution helper for
the unused folders. Its default mode is preview-only and records
`PreviewOnly=true`, `DeletesFiles=false`, `StagesFiles=false`, and
`ModifiesAssets=false`. It only moves folders when `-Execute`,
`-ConfirmReferenceChecks`, and an explicit `-ArchiveRoot` outside the project
are provided. It uses the cleanup-plan candidate list, never performs permanent
deletion, never runs git staging, and should be followed by an editor smoke test
before the local cleanup is considered complete.
After a local archive run, those folders should disappear from the current
cleanup-candidate list. That is an expected local state, not a repository
deletion to commit. The pre-commit summary reports the remaining current cleanup
candidate count, `PresentKnownUnusedCleanupCandidateCount`, and the known
unused-candidate max/absent-or-archived count so the local cleanup can be
tracked without staging large asset folders.
`export_unused_content_archive_evidence.ps1` verifies the post-archive local
state. It checks that known unused folders are absent from the Unreal project,
present under the archive root, the archive root is outside the project, archive
files are not staged, and DTCore has not been touched. This evidence is
local-only: it is not repository acceptance, deletion approval, or permission to
stage archived asset folders.
`export_sample_content_decision_report.ps1` gives the copied
`Samples/PixelStreaming` folder the same read-only treatment: it records
`RecommendedDecision=KeepLocalUnlessOwned`, `MustRemainUntracked=true`,
`SafeToStage=false`, and setup-documentation alternative steps until project
ownership, license/redistribution approval, and the documentation alternative
decision are recorded. The repository-side alternative is
`docs/pixel_streaming_setup.md`; keep that document current instead of staging
copied sample files.
