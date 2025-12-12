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
        self.chat_area.tag_config("self", foreground="blue")
        self.chat_area.tag_config("server", foreground="black")
        self.chat_area.tag_config("user", foreground="green")

        self.entry = tk.Entry(self.window, font=("Arial", 12))
        self.entry.pack(padx=10, pady=(0, 10), fill=tk.X)
        self.entry.bind("<Return>", self.send_message)


        # connection message in chat
        self.add_text(f"Connected to the chat.\n", tag="server")
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

    def add_text(self, msg, tag=None):
        self.chat_area.config(state="normal")
        if tag:
            self.chat_area.insert(tk.END, msg, tag)
        else:
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

        self.add_text(f"You: {msg}\n", tag="self")
        self.entry.delete(0, tk.END)

    def receive_messages(self):
        buffer = ""

        while True:
            try:
                data = self.sock.recv(BUFFER_SIZE)

                # socket close
                if data == b"":
                    self.add_text("[Server closed connection]\n", "server")
                    break
                
                chunk = data.decode(errors="ignore")
                buffer += chunk

                # examine full lines
                while "\n" in buffer:
                    text, buffer = buffer.split("\n", 1)
                    text = text.strip()

                    if not text:
                        continue 

                    # find username prefix
                    if ":" in text:
                        sender, msg = text.split(":", 1)
                        sender = sender.strip()
                        msg = msg.strip()

                        if sender.lower() == "server":
                            self.add_text(f"{sender}: {msg}\n", tag="server")
                        elif sender == self.username:
                            self.add_text(f"{sender}: {msg}\n", tag="self")
                        else:
                            self.add_text(f"{sender}: {msg}\n", tag="user")
                    else:
                        # no username (raw text) (???)
                        self.add_text(text + "\n")              
            
            except:
                break

        self.add_text("[Disconnected from server]\n", tag="server")
        self.window.after(0, self.show_disconnect_popup())
        exit(0)

    def show_disconnect_popup(self):
        popup = tk.Toplevel(self.window)
        popup.title("Disconnected")

        popup.geometry("300x120")
        popup.resizable(False, False)

        label = tk.Label(popup, text="Disconnected from server", font = ("Arial", 12))
        label.pack(pady=15)

        quit_button = tk.Button(popup, text="Quit", font=("Arial", 12), command=self.window.quit)

        quit_button.pack(pady=5)

        popup.transient(self.window) 
        popup.grab_set()


if __name__ == "__main__":
    ChatClient()
