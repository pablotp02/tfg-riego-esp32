import os
import smtplib
from email.mime.text import MIMEText


def send_email(subject: str, body: str, to_address: str) -> bool:
    """
    Envía un correo electrónico mediante una cuenta de Gmail,
    autenticándose con una contraseña de aplicación (no la contraseña
    normal de la cuenta, bloqueada por Google desde 2022 para este uso).
    Devuelve True si el envío tuvo éxito, False en caso contrario. No
    lanza excepciones: un fallo en el envío de la notificación nunca
    debe interrumpir el flujo principal del sistema.
    """
    gmail_user = os.getenv("GMAIL_USER")
    gmail_password = os.getenv("GMAIL_APP_PASSWORD")

    if not gmail_user or not gmail_password:
        print("[EMAIL] Faltan GMAIL_USER o GMAIL_APP_PASSWORD en el entorno")
        return False

    msg = MIMEText(body)
    msg["Subject"] = subject
    msg["From"] = gmail_user
    msg["To"] = to_address

    try:
        server = smtplib.SMTP_SSL("smtp.gmail.com", 465, timeout=10)
        server.login(gmail_user, gmail_password)
        server.send_message(msg, to_addrs=[to_address])
        server.quit()
        return True
    except smtplib.SMTPException as e:
        print(f"[EMAIL] Error SMTP al enviar: {e}")
        return False
    except Exception as e:
        print(f"[EMAIL] Excepción al enviar email: {e}")
        return False