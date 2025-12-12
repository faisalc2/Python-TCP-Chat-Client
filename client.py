import socket
import threading
import tkinter as tk
from tkinter import simpledialog, scrolledtext, messagebox

import ServerLoginDialog

BUFFER_SIZE = 1024


class ChatClient:
    def __init__(self):
        self.sock = None
        self.username = None
        self.server_host = None
        self.server_port = None

        # connect and request username
        if not self.connect_and_login():
            return  # stop if fail

        # open chat window
        self.window = tk.Tk()
        self.window.title(f"Chat - {self.username}")

        self.chat_area = scrolledtext.ScrolledText(self.window, wrap=tk.WORD, state="disabled")
        self.chat_area.pack(padx=10, pady=10, fill=tk.BOTH, expand=True)

        self.entry = tk.Entry(self.window, font=("Arial", 12))
        self.entry.pack(padx=10, pady=(0, 10), fill=tk.X)
        self.entry.bind("<Return>", self.send_message)

        # thread to listen
        threading.Thread(target=self.receive_messages, daemon=True).start()

        self.window.mainloop()

    def connect_and_login(self):
        root = tk.Tk()
        root.withdraw()

        login_dialog = ServerLoginDialog.ServerLoginDialog(root)
        server_host, server_port, username = login_dialog.result

        self.server_host = server_host
        self.server_port = int(server_port)
        self.username = username

        # try to connect
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.server_host, self.server_port))
        except Exception as e:
            messagebox.showerror("Connection Failed", f"Could not connect: {e}")
            return False

        # read welcome message
        prompt_buffer = ""
        try:
            while "Enter username" not in prompt_buffer:
                chunk = self.sock.recv(BUFFER_SIZE).decode()
                if not chunk:
                    messagebox.showerror("ERROR", "Server connection closed.")
                    return False
                prompt_buffer += chunk

        except Exception as e:
            messagebox.showerror("Error", f"Failed to read login prompt: {e}")
            return False


        # send username
        try:
            self.sock.sendall((self.username + "\n").encode())
        except:
            messagebox.showerror("Error", "Failed to send username.")
            return False

        root.destroy()
        return True

    def add_text(self, msg):
        self.chat_area.config(state="normal")
        self.chat_area.insert(tk.END, msg)
        self.chat_area.config(state="disabled")
        self.chat_area.yview(tk.END)

    def send_message(self, event=None):
        msg = self.entry.get().strip()
        if not msg:
            return

        try:
            self.sock.sendall((msg + "\n").encode())
        except:
            self.add_text("[Error sending message]\n")
            return

        self.add_text(f"You: {msg}\n")
        self.entry.delete(0, tk.END)

    def receive_messages(self):
        while True:
            try:
                data = self.sock.recv(BUFFER_SIZE)
                if not data:
                    break
                self.add_text(data.decode())
            except:
                break

        self.add_text("[Disconnected from server]\n")


if __name__ == "__main__":
    ChatClient()
