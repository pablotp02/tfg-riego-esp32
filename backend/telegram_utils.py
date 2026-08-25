import os
import requests

TELEGRAM_API_URL = "https://api.telegram.org/bot{token}/sendMessage"


def send_telegram_message(text: str, chat_id: str) -> bool:
    """
    Envía un mensaje de texto por Telegram al chat_id especificado.
    Devuelve True si el envío tuvo éxito, False en caso contrario.
    No lanza excepciones: un fallo en el envío de la notificación
    nunca debe interrumpir el flujo principal del sistema (por
    ejemplo, la recepción de un ciclo desde la ESP32).
    """
    token = os.getenv("TELEGRAM_BOT_TOKEN")

    if not token or not chat_id:
        print("[TELEGRAM] Falta TELEGRAM_BOT_TOKEN en el entorno, o no se especificó chat_id")
        return False

    url = TELEGRAM_API_URL.format(token=token)
    payload = {
        "chat_id": chat_id,
        "text": text
    }

    try:
        response = requests.post(url, data=payload, timeout=5)
        if response.status_code == 200:
            return True
        else:
            print(f"[TELEGRAM] Error al enviar: {response.status_code} - {response.text}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"[TELEGRAM] Excepción al enviar mensaje: {e}")
        return False