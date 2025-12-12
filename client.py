import socket
import threading
import tkinter as tk
from tkinter import simpledialog, scrolledtext, messagebox

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 5555
BUFFER_SIZE = 1024


class ChatClient:
    def __init__(self):
        self.sock = None
        self.username = None

        # connect and request username
        if not self.connect_and_login():
            return  # Stop if failed to connect or login

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
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((SERVER_HOST, SERVER_PORT))
        except Exception as e:
            messagebox.showerror("Connection Failed", f"Could not connect: {e}")
            return False

        # grab username prompt
        try:
            prompt = self.sock.recv(BUFFER_SIZE).decode()
        except:
            messagebox.showerror("Error", "Failed to read from server.")
            return False

        #  get username
        root = tk.Tk()
        root.withdraw()  # Hide small root window

        self.username = simpledialog.askstring("Login", prompt)

        if not self.username:
            messagebox.showwarning("Login Failed", "You must enter a username.")
            self.sock.close()
            return False

        # send username
        try:
            self.sock.sendall((self.username + "\n").encode())
        except:
            messagebox.showerror("Error", "Failed to send username to server.")
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
