amixer -q sset 'Headphone Mux' 'LINE_IN'
amixer -q sset 'Headphone' 'off'
amixer -q sset 'Headphone Playback ZC' 'off'
amixer -q sset 'PCM' 150 # 0 - 192
amixer -q sset 'Lineout' 25  # 0 - 31
amixer -q sset 'Lineout' 'on'
amixer -q sset 'Mic' 0
amixer -q sset 'Capture' 2 #0 - 15
amixer -q sset 'Capture Attenuate Switch (-6dB)' 'off' #0 - 15
amixer -q sset 'Capture Mux' 'LINE_IN'
amixer -q sset 'Capture ZC' 'off'
amixer -q sset 'AVC' 'on'
amixer -q sset 'AVC Hard Limiter' 'off'
amixer -q sset 'AVC Integrator Response' 3 # 0 - 3
amixer -q sset 'AVC Max Gain' 0 # 0 - 2
amixer -q sset 'AVC Threshold' 0 # 0 - 96 db   #limiter only mode
amixer -q sset 'BASS 0' 40 # 0 - 95
amixer -q sset 'BASS 1' 40 # 0 - 95
amixer -q sset 'BASS 2' 40 # 0 - 95
amixer -q sset 'BASS 3' 60 # 0 - 95
amixer -q sset 'BASS 4' 70 # 0 - 95
amixer -q sset 'DAP MIX Mux' 'ADC'
amixer -q sset 'DAP Main channel' 65535  #0 - 65535
amixer -q sset 'DAP Mix channel' 0  #0 - 65535
amixer -q sset 'DAP Mux' 'ADC'
amixer -q sset 'Digital Input Mux' 'I2S'
echo Amixer Done
