# assets/

Vazio de propósito.

Sinais de teste **não** são versionados aqui: eles são gerados sob demanda e de
forma determinística por `soundwave gen`, o que os torna reproduzíveis sem
ocupar espaço no repositório.

```bash
soundwave gen sine     assets/a440.wav      --freq 440 --duration 2
soundwave gen chord    assets/acorde.wav    --freq 261.63
soundwave gen harmonics assets/harmonicos.wav --freq 110 --harmonics 8
soundwave gen sweep    assets/varredura.wav --duration 5
soundwave gen noise    assets/ruido.wav
soundwave gen silence  assets/silencio.wav
```

O `.gitignore` abre exceção para `assets/*.wav` caso você queira versionar
deliberadamente algum arquivo pequeno de referência.

Para analisar sem gerar arquivo nenhum, use a pseudo-URI do CLI:

```bash
soundwave analyze "gen:sweep:0:3" --out out/
```
