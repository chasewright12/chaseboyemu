#!/usr/bin/env python3
"""Diagnóstico independente do DualShock 4 no Linux/WSL.

Uso:
  python3 test_dualshock.py --list
  python3 test_dualshock.py --live
  python3 test_dualshock.py --simulate

Não altera os arquivos do Chase Boy Emulator.
"""
import argparse
import glob
import os
import shutil
import subprocess
import time

# Mapeamento editável para a simulação (não é enviado ao controle físico).
DEFAULT_MAPPING = {
    "cross": "b",
    "circle": "a",
    "square": "unused",
    "triangle": "unused",
    "dpad_up": "up",
    "dpad_down": "down",
    "dpad_left": "left",
    "dpad_right": "right",
    "l1": "unused",
    "r1": "unused",
    "share": "select",
    "options": "start",
    "l3": "unused",
    "r3": "unused",
    "ps": "unused",
    "touchpad": "unused",
}
LOGICAL_BUTTONS = {"up", "down", "left", "right", "a", "b", "select", "start"}

def command_output(args):
    try:
        p = subprocess.run(args, text=True, capture_output=True, timeout=5)
        return p.returncode, (p.stdout + p.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as exc:
        return 127, str(exc)

def list_devices():
    print("=== Diagnóstico USB / input no Linux/WSL ===")
    try:
        version = open("/proc/version", errors="ignore").read().lower()
    except OSError:
        version = ""
    print("WSL:", "sim" if "microsoft" in version else "não detectado")

    print("\n[1] lsusb")
    code, output = command_output(["lsusb"])
    print(output or ("lsusb não instalado ou sem saída." if code else "(sem dispositivos listados)"))
    if code == 0:
        matches = [line for line in output.splitlines()
                   if any(word in line.lower() for word in ("sony", "dualshock", "054c", "wireless controller"))]
        print("\nPossíveis linhas do controle:", "\n".join(matches) if matches else "nenhuma identificada pelo nome")

    events = sorted(glob.glob("/dev/input/event*"))
    joysticks = sorted(glob.glob("/dev/input/js*"))
    print("\n[2] Nós de entrada")
    print("event:", ", ".join(events) if events else "nenhum")
    print("js:", ", ".join(joysticks) if joysticks else "nenhum")

    print("\n[3] Dispositivos Sony/PlayStation em /proc/bus/input/devices")
    try:
        blocks = open("/proc/bus/input/devices", errors="replace").read().split("\n\n")
        matches = [b.strip() for b in blocks if any(
            w in b.lower() for w in ("sony", "dualshock", "playstation", "wireless controller"))]
        print("\n\n".join(matches) if matches else "nenhuma entrada com nome correspondente")
    except OSError as exc:
        print("Não foi possível ler:", exc)

    print("\n[4] Bibliotecas/ferramentas")
    print("evtest:", shutil.which("evtest") or "não instalado")
    print("jstest:", shutil.which("jstest") or "não instalado")
    try:
        import evdev
        print("python evdev: instalado")
        print("Dispositivos evdev:", evdev.list_devices())
    except ImportError:
        print("python evdev: não instalado")
    try:
        import pygame
        print("pygame:", pygame.version.ver)
    except ImportError:
        print("pygame: não instalado")

    print("\nLeitura rápida:")
    print("- Se lsusb não listar o controle, o USB provavelmente não está disponível no WSL.")
    print("- Se lsusb listar, mas não houver dispositivo de entrada associado, investigue driver/permissões.")
    print("- Se houver dispositivo de entrada, tente: python3 test_dualshock.py --live")
    return 0

def live_evdev():
    try:
        import evdev
    except ImportError:
        return False

    devices = []
    for path in evdev.list_devices():
        try:
            dev = evdev.InputDevice(path)
            if any(w in (dev.name or "").lower()
                   for w in ("sony", "dualshock", "playstation", "wireless controller")):
                devices.append(dev)
            else:
                dev.close()
        except OSError:
            pass

    if not devices:
        print("evdev instalado, mas não encontrei dispositivo com nome Sony/PlayStation.")
        return True

    print("Controles encontrados:")
    for i, dev in enumerate(devices):
        print(f"  [{i}] {dev.path}: {dev.name}")
    index = 0
    if len(devices) > 1:
        raw = input("Qual dispositivo? [0] ").strip()
        if raw:
            try:
                index = int(raw)
                if not 0 <= index < len(devices):
                    raise ValueError
            except ValueError:
                print("Índice inválido.")
                for d in devices:
                    d.close()
                return True

    dev = devices[index]
    print(f"Lendo {dev.name} ({dev.path}). Aperte botões/mova analógicos; Ctrl+C encerra.")
    try:
        for event in dev.read_loop():
            if event.type == 1:  # EV_KEY
                label = evdev.ecodes.KEY.get(event.code, f"KEY_{event.code}")
                state = {0: "solto", 1: "pressionado", 2: "repetição"}.get(event.value, str(event.value))
                print(f"BOTÃO {label}: {state}")
            elif event.type == 3:  # EV_ABS
                label = evdev.ecodes.ABS.get(event.code, f"ABS_{event.code}")
                print(f"EIXO {label}: {event.value}")
    except KeyboardInterrupt:
        print("\nTeste encerrado.")
    except OSError as exc:
        print(f"Erro lendo dispositivo: {exc}")
    finally:
        for d in devices:
            d.close()
    return True

def live_pygame():
    try:
        import pygame
    except ImportError:
        return False

    pygame.init()
    pygame.joystick.init()
    count = pygame.joystick.get_count()
    if count == 0:
        print("pygame não detectou nenhum joystick/controle.")
        pygame.quit()
        return True

    sticks = []
    for i in range(count):
        stick = pygame.joystick.Joystick(i)
        stick.init()
        sticks.append(stick)
        print(f"[{i}] {stick.get_name()} — botões={stick.get_numbuttons()}, "
              f"eixos={stick.get_numaxes()}, hats={stick.get_numhats()}")

    print(f"Lendo {sticks[0].get_name()}. Aperte botões/mova analógicos; Ctrl+C encerra.")
    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.JOYBUTTONDOWN:
                    print(f"BOTÃO {event.button}: pressionado")
                elif event.type == pygame.JOYBUTTONUP:
                    print(f"BOTÃO {event.button}: solto")
                elif event.type == pygame.JOYAXISMOTION:
                    print(f"EIXO {event.axis}: {event.value:+.3f}")
                elif event.type == pygame.JOYHATMOTION:
                    print(f"DPAD/HAT {event.hat}: {event.value}")
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nTeste encerrado.")
    finally:
        pygame.quit()
    return True

def simulate():
    mapping = DEFAULT_MAPPING.copy()
    print("=== Simulação de comandos ===")
    print("Não precisa de controle físico. Mapeamento inicial:")
    for physical, logical in mapping.items():
        print(f"  {physical:12} -> {logical}")
    print("\nComandos lógicos: up, down, left, right, a, b, select, start")
    print("Digite 'map' para editar um mapeamento, 'q' para sair.")
    while True:
        try:
            cmd = input("sim> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if cmd in {"q", "quit", "exit"}:
            break
        if cmd == "map":
            physical = input("Botão físico (ex.: cross, circle, dpad_up): ").strip().lower()
            logical = input("Comando lógico (up/down/left/right/a/b/select/start/unused): ").strip().lower()
            if physical not in mapping:
                print("Botão físico desconhecido.")
            elif logical not in LOGICAL_BUTTONS | {"unused"}:
                print("Comando lógico inválido.")
            else:
                mapping[physical] = logical
                print(f"OK: {physical} -> {logical}")
        elif cmd in LOGICAL_BUTTONS:
            print(f"SIMULADO: '{cmd}' pressionado -> solto")
        else:
            print("Comando desconhecido.")
    print("Simulação encerrada.")
    return 0

def main():
    parser = argparse.ArgumentParser(description="Teste DualShock 4 independente do emulador")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--list", action="store_true", help="diagnosticar USB e dispositivos de entrada")
    group.add_argument("--live", action="store_true", help="ler eventos reais do controle")
    group.add_argument("--simulate", action="store_true", help="simular comandos sem hardware")
    args = parser.parse_args()
    if args.list:
        return list_devices()
    if args.simulate:
        return simulate()
    if args.live:
        if live_evdev():
            return 0
        if live_pygame():
            return 0
        print("Para ler o controle, instale uma biblioteca em um ambiente virtual:")
        print("  python3 -m venv .venv && . .venv/bin/activate")
        print("  pip install evdev")
        print("ou instale pygame no mesmo ambiente.")
        return 2
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
