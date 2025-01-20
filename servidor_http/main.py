from http.server import BaseHTTPRequestHandler, HTTPServer

# Clase manejadora de solicitudes HTTP
class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Sobrescribe el método para evitar imprimir en consola
        pass
    
    def do_GET(self):
        # Enviar respuesta
        self.send_response(200)  # Código de estado HTTP 200 (OK)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()
        self.wfile.write(b"Status: OK")  # Respuesta al cliente

    def do_POST(self):
        # Imprimir detalles de la solicitud POST en la consola
        content_length = int(self.headers.get('Content-Length', 0))  # Longitud del cuerpo
        post_data = self.rfile.read(content_length).decode('utf-8')
        print(post_data)

        # Enviar respuesta
        self.send_response(200)  # Código de estado HTTP 200 (OK)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()
        self.wfile.write(b"Status: OK")  # Respuesta al cliente


def run(server_class=HTTPServer, handler_class=SimpleHTTPRequestHandler, port=3080):
    # Configurar el servidor
    server_address = ('', port)  # Escucha en todas las interfaces
    httpd = server_class(server_address, handler_class)
    print(f"Servidor HTTP corriendo en el puerto {port}...")
    httpd.serve_forever()


if __name__ == "__main__":
    run()
