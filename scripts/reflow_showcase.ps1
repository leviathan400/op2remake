# Re-flow opu_12_showcase.opm to fit the 128x128 map with a 1-tile gap between every
# object, and drop duplicate units (the 8 directional copies collapse to ONE per distinct
# Type+weapon/cargo combo; all face East). Buildings (footprint > 1x1) are laid out in a
# footprint-aware band below the 1x1 units. Positions are 1-based game coords; a building's
# Position is its footprint CENTRE (top-left + size/2), matching OP2MissionEditor.
param(
  [string]$Opm = "D:\opu\op2remake\OPU\maps\PlainsAndPasses\opu_12_showcase.opm",
  [string]$Catalog = "D:\opu\op2remake\building_anim_catalog.tsv"
)

$d = Get-Content $Opm -Raw | ConvertFrom-Json

# name -> [fpX,fpY] from the building catalog
$fp = @{}
Get-Content $Catalog | Select-Object -Skip 1 | ForEach-Object {
  $c = $_ -split "`t"
  if ($c.Count -ge 3 -and $c[2] -match '^\d+x\d+$') {
    $wh = $c[2] -split 'x'; $fp[$c[1]] = @([int]$wh[0], [int]$wh[1])
  }
}

$minC = 3; $maxC = 123   # 0-based usable cell range (kept off the map edges)

# Dedupe per player by Type+CargoType, face East, split 1x1 vs footprint buildings.
$units = New-Object System.Collections.ArrayList
$bldgs = New-Object System.Collections.ArrayList
foreach ($p in $d.MasterVariant.Players) {
  $seen = @{}; $keep = New-Object System.Collections.ArrayList
  foreach ($u in $p.Resources.Units) {
    $key = "$($u.TypeID)|$($u.CargoType)"
    if ($seen.ContainsKey($key)) { continue }
    $seen[$key] = $true
    $u.Direction = "East"
    [void]$keep.Add($u)
    $isBldg = $fp.ContainsKey($u.TypeID) -and ($fp[$u.TypeID][0] -gt 1 -or $fp[$u.TypeID][1] -gt 1)
    if ($isBldg) { [void]$bldgs.Add($u) } else { [void]$units.Add($u) }
  }
  $p.Resources.Units = @($keep.ToArray())
}

# 1x1 units: simple grid at pitch 2 (object + 1 gap).
$cx = $minC; $cy = $minC
foreach ($u in $units) {
  $u.Position.X = $cx + 1; $u.Position.Y = $cy + 1
  $cx += 2
  if ($cx -gt ($maxC - 1)) { $cx = $minC; $cy += 2 }
}

# Buildings: footprint-aware rows below the units, 1-tile gaps; Position = centre.
$by = $cy + 3; $bx = $minC; $rowH = 0
foreach ($u in $bldgs) {
  $w = $fp[$u.TypeID][0]; $h = $fp[$u.TypeID][1]
  if (($bx + $w) -gt $maxC) { $bx = $minC; $by += $rowH + 1; $rowH = 0 }
  $u.Position.X = $bx + [math]::Floor($w / 2) + 1
  $u.Position.Y = $by + [math]::Floor($h / 2) + 1
  $bx += $w + 1
  if ($h -gt $rowH) { $rowH = $h }
}

"units(1x1)=$($units.Count)  buildings=$($bldgs.Count)  total=$($units.Count + $bldgs.Count)"
"max building row reached Y=$($by + $rowH) (limit $maxC)"

$json = $d | ConvertTo-Json -Depth 40
[System.IO.File]::WriteAllText($Opm, $json, (New-Object System.Text.UTF8Encoding($false)))
"wrote $Opm"
