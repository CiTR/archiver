from mod_python import apache
 
def handler(req):
        req.log_error('handler')
        req.content_type = 'text/html'
        req.send_http_header()
        req.write('<html><head><title>Testing mod_python</title></head><body>')
        req.write('Hello World!')
        req.write('</body></html>')
        return apache.OK

