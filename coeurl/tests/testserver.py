#!/usr/bin/env python3
from flask import Flask, Response, redirect, url_for, request
import sys

app = Flask(__name__)

@app.route("/", methods=['GET'])
def test_get1():
    return Response("OK", mimetype='text/plain')

@app.route("/some/path")
def test_get2():
    return Response("OK", mimetype='text/plain')

@app.route("/redirect")
def test_redirect():
    return redirect(url_for('test_get1'))

@app.route("/double_redirect")
def test_doubleredirect():
    return redirect(url_for('test_redirect'))

@app.route("/post", methods=['POST'])
def test_post():
    return request.get_data();

@app.route("/put", methods=['PUT'])
def test_put():
    return request.get_data();

@app.route("/delete", methods=['DELETE'])
def test_delete():
    return request.get_data();

if __name__ == "__main__":
    if len(sys.argv) >= 2:
        app.run(debug=True, ssl_context=(sys.argv[1]+'/cert.pem', sys.argv[1]+'/key.pem'), port=5443)
    else:
        app.run(debug=True)

