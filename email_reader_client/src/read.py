import os
import base64
import json
import threading
import time
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build

SCOPES = ['https://mail.google.com/']
exit_flag = False

def get_gmail_service():
    creds = None
    token_path = './email_reader_client/config/token.json'
    cred_path = './email_reader_client/config/credentials.json'
    
    if os.path.exists(token_path):
        creds = Credentials.from_authorized_user_file(token_path, SCOPES)
    else:
        flow = InstalledAppFlow.from_client_secrets_file(cred_path, SCOPES)
        creds = flow.run_local_server(port=0)
        with open(token_path, 'w') as token:
            token.write(creds.to_json()) #read to file .json
    return build('gmail', 'v1', credentials=creds)

def get_message_detail(service, msg_id):
    msg = service.users().messages().get(userId='me', id=msg_id, format='full').execute()

    headers = msg['payload']['headers']
    subject = next((h['value'] for h in headers if h['name'] == 'Subject'), '(No Subject)')

    body = ''
    payload = msg['payload']
    if 'parts' in payload:
        for part in payload['parts']:
            if part['mimeType'] == 'text/plain':
                data = part['body'].get('data') 
                if data:
                    body = base64.urlsafe_b64decode(data).decode('utf-8')
                    break
    elif 'body' in payload:
        data = payload['body'].get('data')
        if data:
            body = base64.urlsafe_b64decode(data).decode('utf-8')
        
    return subject, body


def save_to_json(message_id, subject, body, filename='./email_reader_client/info/email.json'):
    data = {
        "subject": subject.replace('\r\n', ''),
        "body": body.replace('\r\n', '') 
    }
    with open(filename, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=4)

def monitor_email():
    global exit_flag
    service = get_gmail_service()
    last_msg_id = None

    while not exit_flag:
        try:
            results = service.users().messages().list(userId='me', maxResults=1, labelIds=['INBOX']).execute()
            messages = results.get('messages', [])

            if messages:
                msg_id = messages[0]['id']
                if msg_id != last_msg_id:
                    subject, body = get_message_detail(service, msg_id)
                    save_to_json(msg_id, subject, body)
                    print(f"The latest message content in gmail has updated in 'email.json'!")
                    last_msg_id = msg_id
        except Exception as e:
            print("Error!", e)

        time.sleep(1)


def wait_for_exit():
    global exit_flag
    input("Press Enter to exit...\n")
    exit_flag = True


if __name__ == '__main__':
    t1 = threading.Thread(target=monitor_email)
    t2 = threading.Thread(target=wait_for_exit)

    t1.start()
    t2.start()
    
    t1.join()
    t2.join()
    
    
    
