
import cv2
import mediapipe as mp
import serial
from dataclasses import dataclass

"""
    MEDIA PIPE INITIALIZATION
"""
mp_hands =  mp.solutions.hands
mp_draw =   mp.solutions.drawing_utils
hands =     mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=2,
    min_detection_confidence=0.7,
    min_tracking_confidence=0.5
)

cap = cv2.VideoCapture(1)

"""
    Initialize UART Serial
"""
ser = serial.Serial(
    port='/dev/cu.usbmodemSDA41F0CE7E1',
    baudrate=115200,
    bytesize=serial.EIGHTBITS,
    timeout=0.01              
)

player_one_move = 0
player_two_move = 0

player_one_tx = b'\x00'
player_two_tx = b'\x00'

last_tx_player_one = None
last_tx_player_two = None

while True:
    # ---- CAMERA PROCESSING ----
    ok, frame = cap.read()
    if not ok:
        break

    h, w = frame.shape[:2]
    center_x = w // 2
    cv2.line(frame, (center_x, 0), (center_x, h), (0, 255, 0), 8)

    frame = cv2.flip(frame, 1)
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    result = hands.process(rgb)

    if result.multi_hand_landmarks:
        for i, hand in enumerate(result.multi_hand_landmarks):
            # Determine if hand is left or right
            handed_label = result.multi_handedness[i].classification[0].label
            if handed_label == "Left":
                player = 1
            else:
                player = 2
            # Draw hand landmarks
            mp_draw.draw_landmarks(
                frame, hand, mp_hands.HAND_CONNECTIONS,
                mp_draw.DrawingSpec(color=(0,255,0), thickness=2, circle_radius=3),
                mp_draw.DrawingSpec(color=(255,0,0), thickness=2)
            )

            index_tip = hand.landmark[8]
            thumb     = hand.landmark[4]
            wrist     = hand.landmark[0]

            index_y = int(index_tip.y * w)
            thumb_y = int(thumb.y * w)
            wrist_y = int(wrist.y * w)

            # UP, DOWN, STOP logic
            if player == 1:
                if abs(index_y - thumb_y) < 150:
                    player_one_move = 0     # stop
                elif thumb_y <= wrist_y:
                    player_one_move = 1     # up
                else:
                    player_one_move = -1    # down
            if player == 2:
                if abs(index_y - thumb_y) < 150:
                    player_two_move = 0     # stop
                elif thumb_y <= wrist_y:
                    player_two_move = 3     # up
                else:
                    player_two_move = 4     # down
            #print(f"P{player} {handed_label} index_y={index_y} thumb_y={thumb_y} wrist_y={wrist_y}")



    cv2.imshow("MediaPipe Hands for PONG", frame)
    if cv2.waitKey(1) == 27:
        break

    # ---- UART SEND FOR PLAYER 1 ----
    if player_one_move == 1:
        player_one_tx = b'\x01'      # UP
    elif player_one_move == -1:
        player_one_tx = b'\x02'      # DOWN
    else:
        player_one_tx = b'\x00'      # STOP

    # Only send if player 1's hand changed
    # ser.write(player_one_tx)
    # print("TX P1:", player_one_tx.hex())

    # ---- PLAYER 2 UART SEND ----
    if player_two_move == 3:
        player_two_tx = b'\x03'
    elif player_two_move == 4:
        player_two_tx = b'\x04'
    else:
        player_two_tx = b'\x00'

    # Only send if player 2's hand changed
   
    # ser.write(player_two_tx)
    packet = 0x00; 
    if player_one_move == 1:
        packet |= 0x01      # P1 UP
    elif player_one_move == -1:
        packet |= 0x02      # P1 DOWN

    if player_two_move == 3:
        packet |= 0x04      # P2 UP
    elif player_two_move == 4:
        packet |= 0x08      # P2 DOWN

    packet = bytes([packet])
    ser.write(packet)
    print("TX PACKET:", packet.hex())
    # print("TX P2:", player_two_tx.hex())


    # ---- UART RECEIVE (debug from MK64F) ----
    rx = ser.read(1)                 # read 1 byte (non-blocking)
    if rx:
        print("RX:", rx.hex())       # print exactly what MCU sends

cap.release()
cv2.destroyAllWindows()
