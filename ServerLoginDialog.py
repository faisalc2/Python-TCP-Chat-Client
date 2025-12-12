import socket
import threading
import tkinter as tk
from tkinter import simpledialog, scrolledtext, messagebox



class ServerLoginDialog(simpledialog.Dialog):
    def body(self, master):
        self.title("Connect to Server")

        tk.Label(master, text="Server Address:").grid(row=0)
        tk.Label(master, text="Port:").grid(row=1)
        tk.Label(master, text="Username:").grid(row=2)

        self.server_entry = tk.Entry(master)
        self.port_entry = tk.Entry(master)
        self.username_entry = tk.Entry(master)

        self.server_entry.insert(0, "127.0.0.1")
        self.port_entry.insert(0, 23)
        
        self.server_entry.grid(row=0, column=1)
        self.port_entry.grid(row=1, column=1)
        self.username_entry.grid(row=2, column=1)

        return self.server_entry
    
    def apply(self):
        server = self.server_entry.get().strip()
        port = self.port_entry.get().strip()
        username = self.username_entry.get().strip()
        self.result = (server, port, username)