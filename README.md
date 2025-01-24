# speculativeNotes
Notes and code storage for a thesis on how the verifier handles speculative side channel attacks

## Spectre proof of concepts used:
[Version 1](https://github.com/crozone/SpectrePoC)
[Version 2](https://github.com/Anton-Cao/spectrev2-poc/blob/master/spectrev2.c)

## Working from paper:
[Spectre paper](https://spectreattack.com/spectre.pdf)

# Setup:
## Machine used:
[CloudLab c8220](https://www.clemson.cloudlab.us/portal/show-hardware.php?type=c8220)
Ubuntu 20.04

## Disable CPU mitigations:
Following [these notes](https://sleeplessbeastie.eu/2020/03/27/how-to-disable-mitigations-for-cpu-vulnerabilities/)

Check mitigations: ```lscpu```

Open ```/etc/default/grub```
Add ```GRUB_CMDLINE_LINUX_DEFAULT="quiet splash mitigations=off"```

```
sudo update-grub
sudo reboot
```

# Misc
We briefly looked into code path tracing using perf:
```sudo perf stat -e cache-misses,cache-references ./spectre_poc```
