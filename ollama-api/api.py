from flask import Flask, request, jsonify
from flask_cors import CORS
import requests
import json

app = Flask(__name__)
CORS(app)

def call_ollama_api(input_data):
    url = "http://localhost:11434/api/generate"

    model = input_data.get("model")
    stream = input_data.get("stream", False)
    messages = input_data.get("messages", [])

    system_message = next((msg['content'] for msg in messages if msg['role'] == 'system'), "")
    user_message = next((msg['content'] for msg in messages if msg['role'] == 'user'), "")
    images = next((msg.get('images', []) for msg in messages if msg['role'] == 'user'), [])
    
    payload = {
        "model": model,
        "prompt": f"{system_message}\n\n{user_message}",
        "stream": stream,
        "images": images
    }
    
    response = requests.post(url, data=json.dumps(payload))

    if response.status_code == 200:
        response_json = response.json()
        print("Ollama API Response:", response_json)
        return response_json.get('response', {})
    else:
        error_response = {
            "error": f"Request failed with status code: {response.status_code}",
            "response": response.text
        }
        print("Ollama API Error Response:", error_response)
        return error_response

@app.route('/', methods=['POST'])
def generate():
    input_data = request.json
    print("input_data:", input_data)
    result = call_ollama_api(input_data)
    return jsonify(result)

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000)
