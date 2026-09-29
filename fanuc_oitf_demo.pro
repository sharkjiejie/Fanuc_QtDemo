QT += widgets sql

TEMPLATE = app
TARGET = FanucOiTF_QtDemo
CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/app/MainWindow.cpp \
    src/core/CoordinateTransform.cpp \
    src/core/GCodeModalEngine.cpp \
    src/core/GCodeParser.cpp \
    src/core/MotionPlanner.cpp \
    src/core/PlcSimulator.cpp \
    src/core/MacroEngine.cpp \
    src/core/MachineController.cpp \
    src/data/AppDatabase.cpp \
    src/data/SamplePrograms.cpp \
    src/ui/CncScreen.cpp \
    src/ui/MachinePanel.cpp \
    src/ui/MdiPanel.cpp \
    src/ui/RotaryDial.cpp

HEADERS += \
    src/app/MainWindow.h \
    src/core/CoordinateTransform.h \
    src/core/GCodeModalEngine.h \
    src/core/GCodeParser.h \
    src/core/MotionPlanner.h \
    src/core/PlcSimulator.h \
    src/core/MacroEngine.h \
    src/core/MachineController.h \
    src/data/AppDatabase.h \
    src/data/SamplePrograms.h \
    src/model/GCodeBlock.h \
    src/model/ModalState.h \
    src/model/MotionSegment.h \
    src/model/PlcState.h \
    src/model/ProgramRecord.h \
    src/model/MachineParameter.h \
    src/model/ToolOffset.h \
    src/model/WorkOffset.h \
    src/ui/CncScreen.h \
    src/ui/MachinePanel.h \
    src/ui/MdiPanel.h \
    src/ui/PanelStyle.h \
    src/ui/RotaryDial.h

INCLUDEPATH += src

msvc {
    QMAKE_CXXFLAGS += /utf-8
}
