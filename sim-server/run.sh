cd ~/ns-allinone-3.42/ns-3.42/sim-server

echo "=== Starting NS-3 Dashboard System ==="

pkill -f "node server.js" 2>/dev/null
pkill -f "python3.*http.server" 2>/dev/null
sleep 2

echo "1. Starting WebSocket server..."
node server.js &
SERVER_PID=$!
sleep 3

echo "2. Starting web server..."
python3 -m http.server 3000 &
WEB_PID=$!
sleep 2

echo ""
echo "✅ Servers are running!"
echo "🌐 WebSocket: ws://localhost:8080"
echo "📊 Dashboard: http://localhost:3000/dashboard.html"
echo ""
echo "Press Ctrl+C to stop"

trap "echo 'Stopping...'; kill $SERVER_PID $WEB_PID 2>/dev/null; exit" INT
wait
