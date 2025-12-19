import cv2
import time
from datetime import datetime

import lib_noy_cv2
import sys
import os

print("=" * 60)
print("🎥 NanoTrack - Система отслеживания объектов с управлением сервоприводами")
print("=" * 60)
print(f"🕐 Запуск: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
print()

# Поднимаемся на уровень выше из папки NanoTrack
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)  # Это будет /home/admin/CRSF
pybind_path = os.path.join(parent_dir, 'pybind')
sys.path.insert(0, pybind_path)

# Импортируем универсальную обертку
# Она сама определит, что использовать: crsf_io_rpi (pybind) или api_server (API)
from crsf_wrapper import CRSFWrapper

# Создаем и инициализируем CRSF
# Программа не знает, через что работает - это прозрачно
crsf = CRSFWrapper()
try:
    crsf.auto_init()
    # Определяем, через что работает (для отчета)
    if hasattr(crsf, '_backend'):
        backend_type = "pybind (crsf_io_rpi)" if crsf._backend == 'pybind' else "API (api_server)"
        print(f"✅ CRSF инициализирован успешно через {backend_type}")
    else:
        print("✅ CRSF инициализирован успешно")
except RuntimeError as e:
    print(f"❌ ОШИБКА: {e}")
    print("⚠️  Продолжаем работу без управления сервоприводами...")
    crsf = None


cameraType = "VideoCaptureUSB" 
#cameraType = "VideoCaptureFile"
#cameraType = "VideoCapturePI"
#cameraType = "VideoCaptureIP"
parametrs = {"width" : 640, "height" : 480, "fps" : 30, "ip" : "rtsp://192.168.1.188:554/stream1", "path" : "video_003.avi"}
capture = lib_noy_cv2.VideoCaptureFactory.create(cameraType, parametrs)


print(f"📹 OpenCV версия: {cv2.__version__}")

#capture = cv2.VideoCapture(0)
time.sleep(0.1)

print(f"📹 Камера открыта: {capture.isOpened()}")

if not capture.isOpened():
    print("❌ ОШИБКА: Камера не открыта! Проверьте подключение камеры.")
    print("   Для USB камеры убедитесь, что она подключена и доступна.")
    print("   Для IP камеры проверьте URL: ", parametrs.get("ip", "не указан"))
    print("   Для файла проверьте путь: ", parametrs.get("path", "не указан"))
else:
    print(f"✅ Камера готова: {cameraType}, разрешение: {parametrs['width']}x{parametrs['height']}, FPS: {parametrs['fps']}")

params = cv2.TrackerNano_Params()
params.backbone = "nanotrack_backbone_sim.onnx"
params.neckhead = "nanotrack_head_sim.onnx"
tracker = cv2.TrackerNano_create(params)
print(f"✅ Трекер NanoTrack создан: backbone={params.backbone}, neckhead={params.neckhead}")

#---------------------------
# Устанавливаем режим работы в manual (если CRSF инициализирован)
if crsf is not None and crsf.is_initialized:
    try:
        crsf.set_work_mode("manual")
        print("📝 Команда записана: setMode manual")
        print("✅ Режим работы установлен: manual")
        # Примечание: setMode не требует send_channels, это отдельная команда
    except Exception as e:
        print(f"❌ Не удалось установить режим работы: {e}")

width = parametrs["width"]
height = parametrs["height"]



boxW = 50
boxH = 50

boxStartPosX = int(width/2) - int(boxW/2)
boxStartPosY = int(height/2) - int(boxH/2)
Sx = 1500
Sy = 1500
Kp = 1
Ux = 0
Uy = 0
Xc = 0
Yc = 0

#---------------------------
box_init = [boxStartPosX,boxStartPosY , boxW, boxH]
box = box_init
state = 0

alpha = 0.25

def setServoX(x):
    """Установить сервопривод X (устаревшая функция, используется прямое управление)"""
    if crsf is not None and crsf.is_initialized:
        crsf.set_channel(3, int(x))

def setServoY(y):
    """Установить сервопривод Y (устаревшая функция, используется прямое управление)"""
    if crsf is not None and crsf.is_initialized:
        crsf.set_channel(4, int(y))

def servoCalc(box):
    Xc = box[0]+ box[2]/2
    Yc = box[1]+ box[3]/2

    Ux = Xc - width/2
    Uy = Yc - height/2

    Sx = 1500 + Kp * Ux
    Sy = 1500 + Kp * Uy

    if Sx > 2000:
        Sx = 2000
    elif Sx < 1000:
        Sx = 1000

    if Sy > 2000:
        Sy = 2000
    elif Sy < 1000:
        Sy = 1000    

    #print("Xc:", Xc, " Yc:", Yc, " Ux:", Ux, " Uy:", Uy, " Sx:", Sx, " Sy:", Sy)

    return Sx,Sy 

def average(box, box_new, alpha):
    box_res = []
    box_res.append(int(round(alpha * box[0] + (1.0 - alpha) * box_new[0])))
    box_res.append(int(round(alpha * box[1] + (1.0 - alpha) * box_new[1])))
    box_res.append(int(round(alpha * box[2] + (1.0 - alpha) * box_new[2])))
    box_res.append(int(round(alpha * box[3] + (1.0 - alpha) * box_new[3])))
    
    return box_res
    
while True:
    ret, frame = capture.read()
    #print("ret = ", ret)
    
    if ret:
        if state == 1:
            tracker.init(frame, box)
            state = 2
            print(f"✅ Трекер инициализирован: box=[{box[0]}, {box[1]}, {box[2]}, {box[3]}]")
            
        elif state == 2:
            flag, box_new = tracker.update(frame)
            if flag:
                box = average(box, box_new, alpha)

                # Вычисляем позицию сервоприводов на основе позиции объекта
                x, y = servoCalc(box)
                
                # Вычисляем центр объекта для отчета
                obj_center_x = box[0] + box[2] // 2
                obj_center_y = box[1] + box[3] // 2
                frame_center_x = width // 2
                frame_center_y = height // 2
                offset_x = obj_center_x - frame_center_x
                offset_y = obj_center_y - frame_center_y
                
                # Управляем сервоприводами через CRSF
                # Программа не знает, через что работает (pybind или API) - это прозрачно
                # Если запущен crsf_io_rpi - команды идут напрямую через pybind
                # Если запущен api_server - команды перехватываются API сервером
                if crsf is not None and crsf.is_initialized:
                    try:
                        # Устанавливаем оба канала
                        crsf.set_channel(3, int(x))
                        print(f"📝 Команда записана: setChannel 3 {int(x)}")
                        crsf.set_channel(4, int(y))
                        print(f"📝 Команда записана: setChannel 4 {int(y)}")
                        
                        # ВАЖНО: отправляем каналы - это фактически отправляет команды в CRSF
                        crsf.send_channels()
                        print(f"✅ Команда отправлена: sendChannels")
                        
                        # Подробный отчет о состоянии
                        print(f"🎯 Объект: центр=({obj_center_x}, {obj_center_y}), смещение=({offset_x:+d}, {offset_y:+d})")
                        print(f"🎮 Сервоприводы: X={x:.1f}, Y={y:.1f}")
                        print(f"📦 Box: [{box[0]}, {box[1]}, {box[2]}, {box[3]}]")
                        print("─" * 60)
                    except Exception as e:
                        print(f"❌ Ошибка отправки команд: {e}")
            else:
                print("⚠️  Трекер потерял объект")
        
        cv2.rectangle(frame, box, (0, 255, 0), 1)
        cv2.imshow("Video", frame)
        key = cv2.waitKey(1)
        
        if key == ord(' '):
            if state == 0:
                box = box_init
                state = 1
                print("▶️  Начало трекинга (нажмите ПРОБЕЛ для остановки)")
                
            elif state == 2:
                box = box_init
                state = 0
                print("⏸️  Трекинг остановлен (нажмите ПРОБЕЛ для запуска)")
    
    else:
        break
            
capture.release()
cv2.destroyAllWindows()

print("✅ Программа завершена")
