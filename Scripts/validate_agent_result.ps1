# Agent result validation: PS 5.1, no external modules. No Editor/build execution.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ResultJson,
    [string]$SchemaPath = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$issues = New-Object 'Collections.Generic.List[string]'
function Has-Property($Object, [string]$Name) {
    return ($null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name])
}
function Check-Node($Value, $Spec, [string]$At) {
    if (Has-Property $Spec 'oneOf') {
        $matches = 0
        foreach ($candidate in @($Spec.oneOf)) {
            $before = $issues.Count
            Check-Node $Value $candidate $At
            if ($issues.Count -eq $before) { $matches++ }
            if ($issues.Count -gt $before) { $issues.RemoveRange($before, $issues.Count - $before) }
        }
        if ($matches -ne 1) { $issues.Add("$At must match exactly one schema variant") }
        return
    }
    $kind = [string]$Spec.type
    $valid = switch ($kind) {
        'object' { $null -ne $Value -and $Value -is [PSCustomObject] }
        'array' { $null -ne $Value -and $Value -is [Array] }
        'string' { $Value -is [string] }
        'boolean' { $Value -is [bool] }
        'number' { $Value -is [byte] -or $Value -is [int16] -or $Value -is [int32] -or $Value -is [int64] -or $Value -is [double] -or $Value -is [decimal] }
        default { $false }
    }
    if (-not $valid) { $issues.Add("$At must have type $kind"); return }
    if ((Has-Property $Spec 'enum') -and $Value -cnotin @($Spec.enum)) { $issues.Add("$At has an invalid enum value") }
    if ($kind -eq 'string' -and (Has-Property $Spec 'minLength') -and [string]::IsNullOrWhiteSpace($Value)) { $issues.Add("$At must not be blank") }
    if ($kind -eq 'object') {
        foreach ($name in @($Spec.required)) {
            if (-not (Has-Property $Value $name)) { $issues.Add("$At.$name is required") }
        }
        foreach ($property in $Value.PSObject.Properties) {
            $name = $property.Name
            if (Has-Property $Spec.properties $name) { Check-Node $property.Value $Spec.properties.$name "$At.$name" }
            elseif ((Has-Property $Spec 'additionalProperties') -and $Spec.additionalProperties -eq $false) { $issues.Add("$At.$name is not allowed") }
        }
    }
    if ($kind -eq 'array') {
        for ($i = 0; $i -lt $Value.Count; $i++) { Check-Node $Value[$i] $Spec.items "$At[$i]" }
    }
}
try {
    $root = Split-Path -Parent $PSScriptRoot
    if (-not [IO.Path]::IsPathRooted($ResultJson)) { $ResultJson = Join-Path $root $ResultJson }
    if (-not $SchemaPath) { $SchemaPath = Join-Path $root 'Tools/agent/schema/result.schema.json' }
    $schema = Get-Content -LiteralPath $SchemaPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $result = Get-Content -LiteralPath $ResultJson -Raw -Encoding UTF8 | ConvertFrom-Json
    Check-Node $result $schema 'result'
} catch { $issues.Add($_.Exception.Message) }
if ($issues.Count) {
    foreach ($issue in $issues) { Write-Output "INVALID: $issue" }
    exit 2
}
Write-Output 'Agent result schema: PASS (required fields, types, enums, object/array structure)'
exit 0
