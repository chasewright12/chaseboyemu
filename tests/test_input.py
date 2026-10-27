#!/usr/bin/env python3
"""
Testes básicos de input.c do Chase Boy Emulator.

Execute na raiz do projeto:
    python3 test_input.py

Por padrão, compila input.c sem USE_SDL e testa input_init/input_set.
Requer um compilador C (gcc ou clang) e os headers reais do projeto.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>
#include "input.h"

/* Captura as chamadas feitas por input_set(), sem precisar inicializar a memória
   completa do emulador. A assinatura deve corresponder à declaração em joypad.h. */
typedef struct {
    Button button;
    int pressed;
} JoypadEvent;

static JoypadEvent events[64];
static int event_count = 0;

void joypad_set(Mem *mem, Button button, int pressed)
{
    (void)mem;
    if (event_count < (int)(sizeof(events) / sizeof(events[0]))) {
        events[event_count].button = button;
        events[event_count].pressed = pressed;
        event_count++;
    }
}

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FALHOU: %s\n", message);
        exit(1);
    }
}

static void reset_events(void)
{
    event_count = 0;
}

static void test_init(void)
{
    Input in;
    input_init(&in, NULL);

    check(in.mem == NULL, "input_init deve guardar o ponteiro Mem recebido");
    for (int i = 0; i < 8; i++) {
        check(in.held[i] == 0, "input_init deve zerar todos os estados held");
    }
    puts("OK: input_init zera o estado");
}

static void test_press_release(void)
{
    Input in;
    input_init(&in, NULL);
    reset_events();

    input_set(&in, BTN_A, SRC_KEYBOARD, 1);
    check(in.held[BTN_A] == SRC_KEYBOARD, "pressionar A deve registrar SRC_KEYBOARD");
    check(event_count == 1, "pressionar A deve gerar uma chamada joypad_set");
    check(events[0].button == BTN_A && events[0].pressed == 1,
          "joypad_set deve receber A pressionado");

    input_set(&in, BTN_A, SRC_KEYBOARD, 0);
    check(in.held[BTN_A] == 0, "soltar A deve limpar a fonte teclado");
    check(event_count == 2, "soltar A deve gerar a segunda chamada");
    check(events[1].button == BTN_A && events[1].pressed == 0,
          "joypad_set deve receber A solto");

    puts("OK: pressionar e soltar botão");
}

static void test_no_duplicate_transitions(void)
{
    Input in;
    input_init(&in, NULL);
    reset_events();

    input_set(&in, BTN_B, SRC_KEYBOARD, 1);
    input_set(&in, BTN_B, SRC_KEYBOARD, 1);
    check(event_count == 1, "pressionar duas vezes não deve duplicar a transição");

    input_set(&in, BTN_B, SRC_KEYBOARD, 0);
    input_set(&in, BTN_B, SRC_KEYBOARD, 0);
    check(event_count == 2, "soltar duas vezes não deve duplicar a transição");

    puts("OK: sem transições duplicadas");
}

static void test_multiple_sources(void)
{
    Input in;
    input_init(&in, NULL);
    reset_events();

    input_set(&in, BTN_RIGHT, SRC_KEYBOARD, 1);
    input_set(&in, BTN_RIGHT, SRC_PAD, 1);
    check(in.held[BTN_RIGHT] == (SRC_KEYBOARD | SRC_PAD),
          "teclado e controle devem coexistir em held");
    check(event_count == 1,
          "adicionar segunda fonte não deve mudar o estado efetivo do botão");

    input_set(&in, BTN_RIGHT, SRC_KEYBOARD, 0);
    check(in.held[BTN_RIGHT] == SRC_PAD,
          "soltar teclado deve manter a fonte controle");
    check(event_count == 1,
          "soltar teclado não deve liberar botão ainda pressionado pelo controle");

    input_set(&in, BTN_RIGHT, SRC_PAD, 0);
    check(in.held[BTN_RIGHT] == 0, "soltar controle deve limpar o estado");
    check(event_count == 2 && events[1].pressed == 0,
          "o botão só deve ser liberado quando a última fonte soltar");

    puts("OK: múltiplas fontes não soltam o botão prematuramente");
}

static void test_sources_are_independent(void)
{
    Input in;
    input_init(&in, NULL);
    reset_events();

    input_set(&in, BTN_START, SRC_PAD, 1);
    input_set(&in, BTN_START, SRC_SCRIPT, 1);
    input_set(&in, BTN_START, SRC_PAD, 0);

    check(in.held[BTN_START] == SRC_SCRIPT,
          "remover SRC_PAD deve preservar SRC_SCRIPT");
    check(event_count == 1,
          "remover uma fonte enquanto outra mantém o botão pressionado não gera transição");

    input_set(&in, BTN_START, SRC_SCRIPT, 0);
    check(event_count == 2 && events[1].pressed == 0,
          "remover a última fonte deve gerar a transição de soltura");

    puts("OK: fontes de entrada são independentes");
}

int main(void)
{
    test_init();
    test_press_release();
    test_no_duplicate_transitions();
    test_multiple_sources();
    test_sources_are_independent();
    puts("\nTodos os testes de input_set passaram.");
    return 0;
}
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Testes unitários básicos de input.c")
    parser.add_argument("--source", default="input.c", help="caminho para input.c (padrão: ./input.c)")
    parser.add_argument("--include-dir", default=None, help="diretório dos headers (padrão: pasta de input.c)")
    parser.add_argument("--cc", default=None, help="compilador C (padrão: gcc ou clang encontrado no PATH)")
    args = parser.parse_args()

    source = Path(args.source).resolve()
    if not source.is_file():
        print(f"Erro: não encontrei {source}", file=sys.stderr)
        print("Execute o script na raiz do projeto ou informe --source caminho/para/input.c.", file=sys.stderr)
        return 2

    include_dir = Path(args.include_dir).resolve() if args.include_dir else source.parent
    compiler = args.cc or shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        print("Erro: nenhum compilador C encontrado. Instale gcc ou clang.", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="chaseboy-input-test-") as temp:
        temp_dir = Path(temp)
        harness_path = temp_dir / "test_input_harness.c"
        binary_path = temp_dir / "test_input"
        harness_path.write_text(HARNESS, encoding="utf-8")

        command = [
            compiler,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-I", str(include_dir),
            str(source),
            str(harness_path),
            "-o", str(binary_path),
        ]
        print("Compilando:", " ".join(command))
        built = subprocess.run(command, text=True, capture_output=True)
        if built.stdout:
            print(built.stdout, end="")
        if built.stderr:
            print(built.stderr, end="", file=sys.stderr)
        if built.returncode:
            print(f"Falha na compilação (código {built.returncode}).", file=sys.stderr)
            print(
                "Se joypad_set tiver uma implementação externa ou assinatura diferente, "
                "ajuste o stub no harness.",
                file=sys.stderr,
            )
            return built.returncode

        result = subprocess.run([str(binary_path)], text=True)
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
