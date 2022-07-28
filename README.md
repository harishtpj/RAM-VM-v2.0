# 🔢 RAM-VM v2.0
[![using-c++](https://img.shields.io/badge/Using-C++%20-blue)](https://gcc.gnu.org/)
[![g++-version](https://img.shields.io/badge/G++-v9.4.0-brightgreen)](https://gcc.gnu.org/)

Welcome to the **RAM-VM v2.0** repository

# ℹ About
RAM-VM v2.0 is a **Fast, RISC Based** Virtual Machine written in C and C++. This VM Uses LC-3 Computer Architecture and therefore it can run LC-3 compatiable Programs .This VM can also be used to create new programming languages and scripts which focuses on *Speed*. This is a updated version of the [RAM-VM](https://github.com/harishtpj/RAM-VM).

# 🛠 Build Instruction
This project uses GNU Make for build
```
git clone https://github.com/harishtpj/RAM-VM-v2.0.git
cd ram-vm-v2.0
make build
```

You may also build the C Version of this VM (vm.c) if you want. Just run ```make build-c``` to compile it.

# 💻 Sample Run
The examples folder contains Example program written and assembled in LC3 asm. To Assemble it on your own, use [laser](https://github.com/PaperFanz/laser) LC-3 assembler
```
user@pc:~/ram-vm-v2.0$ bin/ram-vm examples/hello.obj

Hello World!

RAM-VM Halted
```

# 📝 License

#### Copyright © 2022 [M.V.Harish Kumar](https://github.com/harishtpj). <br>
#### This project is [MIT](https://github.com/harishtpj/RAM-VM-v2.0/blob/main/LICENSE) licensed.