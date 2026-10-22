#!/usr/bin/env python3
"""
hex_to_jpg — Smart House

Objet : reconstituer une image JPEG a partir de l'affichage hexadecimal
        produit par firmware/tests/arducam_capture/arducam_capture.ino.

Usage : python hex_to_jpg.py capture.txt
        python hex_to_jpg.py capture.txt image.jpg

Le fichier texte peut contenir tout l'affichage du moniteur serie :
le script ne garde que ce qui se trouve entre les marqueurs
---DEBUT IMAGE--- et ---FIN IMAGE---.

Aucune dependance : bibliotheque standard de Python uniquement.
"""

import re
import sys
from pathlib import Path

MARQUEUR_DEBUT = "---DEBUT IMAGE---"
MARQUEUR_FIN = "---FIN IMAGE---"


def extraire_bloc(texte: str) -> str:
    """Renvoie la portion situee entre les deux marqueurs."""
    debut = texte.find(MARQUEUR_DEBUT)
    if debut == -1:
        raise ValueError(
            "Marqueur de debut introuvable. Le fichier texte ne contient "
            "pas une capture complete."
        )
    debut += len(MARQUEUR_DEBUT)

    fin = texte.find(MARQUEUR_FIN, debut)
    if fin == -1:
        raise ValueError(
            "Marqueur de fin introuvable. Le transfert a probablement ete "
            "interrompu avant la fin."
        )
    return texte[debut:fin]


def hex_vers_octets(bloc: str) -> bytes:
    """Convertit le bloc hexadecimal en octets, en ignorant les espaces."""
    propre = re.sub(r"[^0-9A-Fa-f]", "", bloc)
    if len(propre) % 2 != 0:
        raise ValueError(
            f"Nombre impair de caracteres hexadecimaux ({len(propre)}) : "
            "une ligne a ete tronquee a la copie."
        )
    return bytes.fromhex(propre)


def verifier(donnees: bytes) -> None:
    """Controle les marqueurs qui ouvrent et ferment un fichier JPEG."""
    if len(donnees) < 4:
        raise ValueError("Image trop courte pour etre un JPEG.")
    if donnees[:2] != b"\xff\xd8":
        raise ValueError("Le fichier ne commence pas par FF D8.")
    if donnees[-2:] != b"\xff\xd9":
        print(
            "  Avertissement : le fichier ne se termine pas par FF D9. "
            "L'image peut etre incomplete.",
            file=sys.stderr,
        )


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    source = Path(sys.argv[1])
    destination = Path(sys.argv[2]) if len(sys.argv) > 2 else source.with_suffix(".jpg")

    try:
        texte = source.read_text(encoding="utf-8", errors="replace")
        donnees = hex_vers_octets(extraire_bloc(texte))
        verifier(donnees)
    except (OSError, ValueError) as erreur:
        print(f"Echec : {erreur}", file=sys.stderr)
        return 1

    destination.write_bytes(donnees)
    print(f"Image ecrite : {destination} ({len(donnees)} octets)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
