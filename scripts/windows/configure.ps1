param(
    [string]$GameDir,
    [ValidateSet("recommended", "full-width-ui", "disabled")]
    [string]$Profile
)
. (Join-Path $PSScriptRoot "common.ps1")
$GameDir = Select-PeaceWalkerGameDir $GameDir
$ini = Join-Path $GameDir $script:IniName
if (-not (Test-Path -LiteralPath $ini)) {
    $defaultIni = Join-Path $script:PackageRoot "config\$script:IniName"
    if (-not (Test-Path -LiteralPath $defaultIni)) { $defaultIni = Join-Path $PSScriptRoot "default.ini" }
    Copy-Item -LiteralPath $defaultIni -Destination $ini
}

if ($Profile) {
    $width = [int](Get-IniValue $ini "Width" "0")
    $height = [int](Get-IniValue $ini "Height" "0")
    switch ($Profile) {
        "recommended" { Write-CleanIni $ini $true $width $height $true $true $true $false }
        "full-width-ui" { Write-CleanIni $ini $true $width $height $true $true $false $false }
        "disabled" { Write-CleanIni $ini $false $width $height $true $true $false $false }
    }
    Set-LauncherBypass $GameDir $false
    Write-Host "Applied profile '$Profile' to $ini"
    exit 0
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PWDisplayMode {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DEVMODE {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public ushort dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
        public uint dmFields; public int dmPositionX, dmPositionY;
        public uint dmDisplayOrientation, dmDisplayFixedOutput;
        public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public ushort dmLogPixels; public uint dmBitsPerPel, dmPelsWidth, dmPelsHeight;
        public uint dmDisplayFlags, dmDisplayFrequency, dmICMMethod, dmICMIntent;
        public uint dmMediaType, dmDitherType, dmReserved1, dmReserved2;
        public uint dmPanningWidth, dmPanningHeight;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern bool EnumDisplaySettings(string name, int mode, ref DEVMODE data);
    public static int[] Current(string name) {
        var d = new DEVMODE { dmDeviceName = new string('\0',32),
            dmFormName = new string('\0',32) };
        d.dmSize = (ushort)Marshal.SizeOf(typeof(DEVMODE));
        if (!EnumDisplaySettings(name, -1, ref d)) return new int[] { 0, 0 };
        return new int[] { (int)d.dmPelsWidth, (int)d.dmPelsHeight };
    }
}
'@
[Windows.Forms.Application]::EnableVisualStyles()
$screen = [Windows.Forms.Screen]::PrimaryScreen
$physical = [PWDisplayMode]::Current($screen.DeviceName)

$currentWidth = [int](Get-IniValue $ini "Width" "0")
$currentHeight = [int](Get-IniValue $ini "Height" "0")
$auto = $currentWidth -le 0 -or $currentHeight -le 0
if ($auto) { $currentWidth = [Math]::Max(640, $physical[0]); $currentHeight = [Math]::Max(480, $physical[1]) }

$form = [Windows.Forms.Form]@{
    Text = "$script:ProductName $script:Version"
    StartPosition = "CenterScreen"; ClientSize = [Drawing.Size]::new(640, 500)
    FormBorderStyle = "FixedDialog"; MaximizeBox = $false
}
$title = [Windows.Forms.Label]@{
    Text = "Ultrawide output with optional centered 16:9 interface"
    Location = [Drawing.Point]::new(24,18); Size = [Drawing.Size]::new(590,32)
    Font = [Drawing.Font]::new("Segoe UI",14,[Drawing.FontStyle]::Bold)
}
$path = [Windows.Forms.Label]@{
    Text = "Game: $GameDir"; Location = [Drawing.Point]::new(26,54)
    Size = [Drawing.Size]::new(585,36); ForeColor = [Drawing.Color]::DimGray
}
$enabled = [Windows.Forms.CheckBox]@{
    Text = "Enable Peace Walker UltraWide Fix"; Location = [Drawing.Point]::new(28,96)
    Size = [Drawing.Size]::new(560,26); Checked = (Get-IniValue $ini "Enabled" "1") -eq "1"
}
$autoBox = [Windows.Forms.CheckBox]@{
    Text = "Use primary monitor resolution ($($physical[0]) x $($physical[1]))"
    Location = [Drawing.Point]::new(28,135); Size = [Drawing.Size]::new(400,25); Checked = $auto
}
$widthLabel = [Windows.Forms.Label]@{Text="Width";Location=[Drawing.Point]::new(46,174);Size=[Drawing.Size]::new(80,24)}
$width = [Windows.Forms.NumericUpDown]@{Location=[Drawing.Point]::new(130,171);Size=[Drawing.Size]::new(130,26)}
$width.Minimum=640; $width.Maximum=16384; $width.Value=$currentWidth
$heightLabel = [Windows.Forms.Label]@{Text="Height";Location=[Drawing.Point]::new(300,174);Size=[Drawing.Size]::new(80,24)}
$height = [Windows.Forms.NumericUpDown]@{Location=[Drawing.Point]::new(380,171);Size=[Drawing.Size]::new(130,26)}
$height.Minimum=480; $height.Maximum=16384; $height.Value=$currentHeight
$letterbox = [Windows.Forms.CheckBox]@{
    Text="Remove original 16:9 side bars";Location=[Drawing.Point]::new(28,220)
    Size=[Drawing.Size]::new(560,25);Checked=(Get-IniValue $ini "RemoveLetterboxing" "1") -eq "1"
}
$fov = [Windows.Forms.CheckBox]@{
    Text="Correct field of view (Hor+, recommended)";Location=[Drawing.Point]::new(28,252)
    Size=[Drawing.Size]::new(560,25);Checked=(Get-IniValue $ini "CorrectFOV" "1") -eq "1"
}
$hud = [Windows.Forms.CheckBox]@{
    Text="Keep HUD, menus and subtitles in a centered 16:9 canvas";Location=[Drawing.Point]::new(28,284)
    Size=[Drawing.Size]::new(590,25);Checked=(Get-IniValue $ini "CenterHUD" "1") -eq "1"
}
$bypass = [Windows.Forms.CheckBox]@{
    Text="Skip the Unity launcher (optional, reversible)";Location=[Drawing.Point]::new(28,316)
    Size=[Drawing.Size]::new(590,25);Checked=(Get-IniValue $ini "BypassUnityLauncher" "0") -eq "1"
}
$notice = [Windows.Forms.Label]@{
    Text="HUD limitation: some pure/long loading illustrations can still stretch to the full output. Disable this option if you prefer the entire interface to use the ultrawide width."
    Location=[Drawing.Point]::new(48,347);Size=[Drawing.Size]::new(550,55)
    ForeColor=[Drawing.Color]::DarkGoldenrod
}
$defaults = [Windows.Forms.Button]@{Text="Recommended";Location=[Drawing.Point]::new(28,405);Size=[Drawing.Size]::new(135,36)}
$save = [Windows.Forms.Button]@{Text="Save and close";Location=[Drawing.Point]::new(374,405);Size=[Drawing.Size]::new(145,36)}
$cancel = [Windows.Forms.Button]@{Text="Cancel";Location=[Drawing.Point]::new(527,405);Size=[Drawing.Size]::new(85,36)}

$updateResolution = { $width.Enabled=-not $autoBox.Checked; $height.Enabled=-not $autoBox.Checked }
$autoBox.Add_CheckedChanged($updateResolution); & $updateResolution
$defaults.Add_Click({
    $enabled.Checked=$true; $autoBox.Checked=$true; $letterbox.Checked=$true
    $fov.Checked=$true; $hud.Checked=$true; $bypass.Checked=$false
})
$cancel.Add_Click({$form.DialogResult=[Windows.Forms.DialogResult]::Cancel;$form.Close()})
$save.Add_Click({
    $w=if($autoBox.Checked){0}else{[int]$width.Value}
    $h=if($autoBox.Checked){0}else{[int]$height.Value}
    if(($w -eq 0) -xor ($h -eq 0)) { [Windows.Forms.MessageBox]::Show("Width and height must both be automatic or both be set.")|Out-Null; return }
    Write-CleanIni $ini $enabled.Checked $w $h $letterbox.Checked $fov.Checked $hud.Checked $bypass.Checked
    Set-LauncherBypass $GameDir $bypass.Checked
    $form.DialogResult=[Windows.Forms.DialogResult]::OK; $form.Close()
})
$form.AcceptButton=$save; $form.CancelButton=$cancel
$form.Controls.AddRange(@($title,$path,$enabled,$autoBox,$widthLabel,$width,$heightLabel,$height,
    $letterbox,$fov,$hud,$bypass,$notice,$defaults,$save,$cancel))
if($form.ShowDialog() -eq [Windows.Forms.DialogResult]::OK) {
    [Windows.Forms.MessageBox]::Show("Settings saved. Start the game normally through Steam.",
        $script:ProductName,"OK","Information")|Out-Null
}
