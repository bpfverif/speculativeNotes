# speculativeNotes
Notes and code storage for a thesis on how the verifier handles speculative side channel attacks

## Spectre proof of concepts used:
[Version 1](https://github.com/crozone/SpectrePoC)
[Version 2](https://github.com/Anton-Cao/spectrev2-poc/blob/master/spectrev2.c)

## eBPF Proof of Concepts
[4.4 and 4.11](https://project-zero.issues.chromium.org/issues/42450298) This project zero submission contains a folder of different PoCs, with one in eBPF for an Intel CPU. I could not get it to pass the verifier. Compile with the included compile.sh. 

[4.17](https://project-zero.issues.chromium.org/issues/42450782) This is also a project zero submission, with one 4.17 PoC. Most testing was done with this file. The uploaded source code contains my "test" function, which can be run by running `./bpf_stuff test <main-cpu> <bounce-cpu> <hex-offset> <hex-length>` rather than `./bpf_stuff <main-cpu> <bounce-cpu> <hex-offset> <hex-length>`. I just copied the loading function and made it always either read from 0x2000 or 0x3000 in order to test the timing, which we only got working on a bare metal CPU (no KVM). Compile with `gcc -pthread -o bpf_stuff bpf_spectrepoc_417.c -Wall -ggdb -std=gnu99`. 

## Working from paper:
[Spectre paper](https://spectreattack.com/spectre.pdf)

Additionally, we located a paper that covered the current mitigations in the verifier, along with a proposed alternative:
[Verifence paper](https://arxiv.org/pdf/2405.00078) 

# Setup:
## Machine used:
[CloudLab c8220](https://www.clemson.cloudlab.us/portal/show-hardware.php?type=c8220)
Ubuntu 20.04

| c8220	|	96 nodes (Ivy Bridge, 20 core) |
| :-------------: | :- |
| CPU	|	Two Intel E5-2660 v2 10-core CPUs at 2.20 GHz (Ivy Bridge) |
| RAM	|	256GB ECC Memory (16x 16 GB DDR4 1600MT/s dual rank RDIMMs |
| Disk |		Two 1 TB 7.2K RPM 3G SATA HDDs |
| NIC	|	Dual-port Intel 10Gbe NIC (PCIe v3.0, 8 lanes |
| NIC	|	Qlogic QLE 7340 40 Gb/s Infiniband HCA (PCIe v3.0, 8 lanes) |

## Kernel versions:

We located a list of current patches in [this keynote](https://popl22.sigplan.org/details/prisc-2022-papers/11/BPF-and-Spectre-Mitigating-transient-execution-attacks).

We settled on a range of vulnerable versions, with the first eBPF PoC using versions 4.4 and 4.11, and the second using 4.17.
Initially we used qemu [(instructions here)](https://github.com/google/syzkaller/blob/master/docs/linux/setup_ubuntu-host_qemu-vm_x86-64-kernel.md) to boot these kernels, but we discovered that it was altering our timing results. If using qemu, be sure to enable all the BPF requirements in the .config file. 

## Disable CPU mitigations:
Following [these notes](https://sleeplessbeastie.eu/2020/03/27/how-to-disable-mitigations-for-cpu-vulnerabilities/)

Check mitigations: ```lscpu```

Open ```/etc/default/grub```
Add ```GRUB_CMDLINE_LINUX_DEFAULT="quiet splash mitigations=off"```

```
sudo update-grub
sudo reboot
```

This was not necessary on the c8220 machine with Ubunutu 18.04!

# Misc
We briefly looked into code path tracing using perf:
```sudo perf stat -e cache-misses,cache-references ./spectre_poc```
