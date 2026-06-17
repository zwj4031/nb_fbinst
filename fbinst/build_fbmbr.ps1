param(
	[string]$MlPath,
	[string]$OutDir = "..\libfbfs"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-Ml {
	if ($MlPath) {
		return (Resolve-Path $MlPath).Path
	}

	$roots = @(
		"C:\Program Files\Microsoft Visual Studio",
		"C:\Program Files (x86)\Microsoft Visual Studio"
	)

	foreach ($root in $roots) {
		if (-not (Test-Path $root)) {
			continue
		}

		$match = Get-ChildItem -Path $root -Recurse -Filter ml.exe -ErrorAction SilentlyContinue |
			Where-Object { $_.FullName -match "\\bin\\Hostx86\\x86\\ml\.exe$" } |
			Sort-Object FullName -Descending |
			Select-Object -First 1
		if ($match) {
			return $match.FullName
		}
	}

	throw "ml.exe not found"
}

function Read-Index {
	param(
		[byte[]]$Data,
		[ref]$Offset
	)

	$b = [int]$Data[$Offset.Value]
	$Offset.Value++
	if (($b -band 0x80) -eq 0) {
		return $b
	}

	$v = (($b -band 0x7f) -shl 8) -bor [int]$Data[$Offset.Value]
	$Offset.Value++
	return $v
}

function Convert-OmfToBin {
	param(
		[string]$ObjPath,
		[string]$BinPath
	)

	$bytes = [IO.File]::ReadAllBytes($ObjPath)
	$pos = 0
	$names = @("")
	$segments = @{}
	$segmentIndex = 0
	$textSegment = 0
	$image = New-Object byte[] 0

	while ($pos -lt $bytes.Length) {
		if ($pos + 3 -gt $bytes.Length) {
			throw "truncated OMF record"
		}

		$type = [int]$bytes[$pos]
		$len = [int]$bytes[$pos + 1] -bor ([int]$bytes[$pos + 2] -shl 8)
		if ($len -lt 1 -or $pos + 3 + $len -gt $bytes.Length) {
			throw ("invalid OMF record at 0x{0:x}" -f $pos)
		}

		$dataStart = $pos + 3
		$dataLen = $len - 1
		$dataEnd = $dataStart + $dataLen
		$data = $bytes[$dataStart..($dataEnd - 1)]

		switch ($type) {
			0x96 {
				$i = 0
				while ($i -lt $data.Length) {
					$n = [int]$data[$i]
					$i++
					$name = ""
					if ($n) {
						$name = [Text.Encoding]::ASCII.GetString($data, $i, $n)
						$i += $n
					}
					$names += $name
				}
			}
			0x98 {
				$segmentIndex++
				$i = 0
				$attr = [int]$data[$i]
				$i++
				$align = ($attr -shr 5) -band 7
				if ($align -eq 0) {
					$i += 3
				}
				$length = [int]$data[$i] -bor ([int]$data[$i + 1] -shl 8)
				$i += 2
				$ref = [ref]$i
				$nameIndex = Read-Index $data $ref
				$i = $ref.Value
				$ref = [ref]$i
				$classIndex = Read-Index $data $ref
				$i = $ref.Value
				$segName = $names[$nameIndex]
				$className = $names[$classIndex]
				$segments[$segmentIndex] = @{
					Name = $segName
					Class = $className
					Length = $length
				}
				if ($segName -eq "_TEXT") {
					$textSegment = $segmentIndex
					$image = New-Object byte[] $length
				}
			}
			0xA0 {
				$i = 0
				$ref = [ref]$i
				$seg = Read-Index $data $ref
				$i = $ref.Value
				$offset = [int]$data[$i] -bor ([int]$data[$i + 1] -shl 8)
				$i += 2
				if ($seg -eq $textSegment) {
					$count = $data.Length - $i
					if ($offset + $count -gt $image.Length) {
						throw "LEDATA exceeds _TEXT segment"
					}
					[Array]::Copy($data, $i, $image, $offset, $count)
				}
			}
		}

		$pos += 3 + $len
	}

	if ($textSegment -eq 0 -or $image.Length -eq 0) {
		throw "_TEXT segment not found"
	}

	[IO.File]::WriteAllBytes($BinPath, $image)
}

function Convert-BinToHeader {
	param(
		[string]$BinPath,
		[string]$HeaderPath,
		[string]$Symbol
	)

	$data = [IO.File]::ReadAllBytes($BinPath)
	$lines = New-Object System.Collections.Generic.List[string]
	$lines.Add("static const unsigned char $Symbol[$($data.Length)] = {")
	for ($i = 0; $i -lt $data.Length; $i += 20) {
		$end = [Math]::Min($i + 20, $data.Length)
		$items = for ($j = $i; $j -lt $end; $j++) {
			[uint32]$data[$j]
		}
		$suffix = if ($end -lt $data.Length) { "," } else { "" }
		$lines.Add("	" + ($items -join ",") + $suffix)
	}
	$lines.Add("};")
	[IO.File]::WriteAllLines($HeaderPath, $lines, [Text.Encoding]::ASCII)
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outPath = Resolve-Path (Join-Path $scriptDir $OutDir)
$workPath = Join-Path ([IO.Path]::GetTempPath()) "fbinst-fbmbr"
New-Item -ItemType Directory -Force -Path $workPath | Out-Null
$ml = Find-Ml

$variants = @(
	@{ Name = "rel"; Symbol = "fb_mbr_rel"; Defines = @() },
	@{ Name = "dbg"; Symbol = "fb_mbr_dbg"; Defines = @("/DDEBUG", "/DDEBUG_INT13") }
)

foreach ($variant in $variants) {
	$obj = Join-Path $workPath ("fb_{0}.obj" -f $variant.Name)
	$lst = Join-Path $workPath ("fb_{0}.lst" -f $variant.Name)
	$bin = Join-Path $workPath ("fb_{0}.mbr" -f $variant.Name)
	$header = Join-Path $outPath ("fb_mbr_{0}.h" -f $variant.Name)
	$args = @("/nologo", "/c", "/omf", "/Cp", "/W3", "/WX") +
		$variant.Defines +
		@("/Fl$lst", "/Fo$obj", (Join-Path $scriptDir "fbmbr.asm"))
	& $ml @args
	if ($LASTEXITCODE -ne 0) {
		throw "ml.exe failed for $($variant.Name)"
	}
	Convert-OmfToBin $obj $bin
	Convert-BinToHeader $bin $header $variant.Symbol
}
