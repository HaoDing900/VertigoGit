# Run from the repository root. The original source stays unchanged.
$starts = @('0', '0.539773', '1.079546')
# Normalize each faded mono segment to approximately -12 dBFS peak.
$gains = @('40.96', '8.587', '31.629')
for ($i = 0; $i -lt 3; $i++) {
    $output = 'Content/SFX/SaArmor/SW_SaArmor_RunStart_{0:00}.wav' -f ($i + 1)
    & ffmpeg -hide_banner -loglevel error -y -ss $starts[$i] -i Content/SFX/SaArmor/360694__postworkflow__janglingkeys-sauer.wav -t 0.539773 -ac 1 -c:a pcm_s16le -af ('afade=t=in:st=0:d=0.005,afade=t=out:st=0.24:d=0.299773,volume=' + $gains[$i]) $output
    if ($LASTEXITCODE -ne 0) { throw 'Audio preparation failed' }
}
