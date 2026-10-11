import dash
from dash import dcc, html
from dash.dependencies import Input, Output
import plotly.graph_objs as go
import requests

IP_AWS = "54.160.234.67"
ENTITY_ID = "urn:ngsi-ld:Lamp:001" # Troque para Lamp:200 se estiver usando a 200

app = dash.Dash(__name__)
app.layout = html.Div([
    html.H2("Histórico de Luminosidade - Smart Lamp (FIWARE)"),
    dcc.Graph(id='grafico-luminosidade'),
    dcc.Interval(id='intervalo', interval=3000, n_intervals=0)
])

@app.callback(Output('grafico-luminosidade', 'figure'), [Input('intervalo', 'n_intervals')])
def atualizar_grafico(n):
    url = f"http://{IP_AWS}:8666/STH/v1/contextEntities/type/Lamp/id/{ENTITY_ID}/attributes/luminosity?lastN=30"
    headers = {'fiware-service': 'smart', 'fiware-servicepath': '/'}
    r = requests.get(url, headers=headers)
    dados = r.json()['contextResponses'][0]['contextElement']['attributes'][0]['values']
    tempos = [f"{(int(item['recvTime'][11:13]) - 3) % 24:02d}{item['recvTime'][13:19]}" for item in dados]
    valores = [float(item['attrValue']) for item in dados]
    return {'data': [go.Scatter(x=tempos, y=valores, mode='lines+markers', name='Luminosidade')]}

if __name__ == '__main__':
    app.run(debug=True, port=8050)