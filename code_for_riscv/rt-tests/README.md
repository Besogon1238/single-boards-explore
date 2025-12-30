# Тестирование Lichee RV Dock 

Текущая реализация предполагает собой схему, при которой Lichee RV Dock (при желании и небольших изменениях любая другая плата) - исследуемая плата, а Arduino Mega 2560 - инструмент тестирования.

![alt text](/pictures/pinout.png)

[Код для Lichee](/code_for_riscv/rt-tests/lichee_example.c)

Собрать код для Lichee:

```
gcc test_lichee.c -lgpiod -o test_lichee
```

Схема подключения:

 Arduino(pin 7) → Lichee (pin 40)
 Lichee (pin 39) →  Arduino(pin 19)

 [Код для Arduino](/code_for_riscv/rt-tests/arduino_example.ino)
