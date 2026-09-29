cmake_minimum_required(VERSION 3.16)

project(FanucOiTF_QtDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets Sql QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Sql)
endif()

add_executable(FanucOiTF_QtDemo
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/core/MachineController.cpp
    src/core/MachineController.h
    src/core/GCodeParser.cpp
    src/core/GCodeParser.h
    src/core/GCodeModalEngine.cpp
    src/core/GCodeModalEngine.h
    src/core/MotionPlanner.cpp
    src/core/MotionPlanner.h
    src/core/MacroEngine.cpp
    src/core/MacroEngine.h
    src/core/PlcSimulator.cpp
    src/core/PlcSimulator.h
    src/core/CoordinateTransform.cpp
    src/core/CoordinateTransform.h
    src/data/AppDatabase.cpp
    src/data/AppDatabase.h
    src/data/SamplePrograms.cpp
    src/data/SamplePrograms.h
    src/model/ProgramRecord.h
    src/model/MachineParameter.h
    src/model/GCodeBlock.h
    src/model/ModalState.h
    src/model/MotionSegment.h
    src/model/PlcState.h
    src/model/ToolOffset.h
    src/model/WorkOffset.h
    src/ui/CncScreen.cpp
    src/ui/CncScreen.h
    src/ui/MachinePanel.cpp
    src/ui/MachinePanel.h
    src/ui/MdiPanel.cpp
    src/ui/MdiPanel.h
    src/ui/PanelStyle.h
    src/ui/RotaryDial.cpp
    src/ui/RotaryDial.h
)

target_include_directories(FanucOiTF_QtDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

enable_testing()

add_executable(FanucGCodeParserTest
    tests/GCodeParserTest.cpp
    src/core/GCodeParser.cpp
    src/core/GCodeParser.h
    src/model/GCodeBlock.h
)
target_include_directories(FanucGCodeParserTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucGCodeParserTest PRIVATE Qt6::Core)
else()
    target_link_libraries(FanucGCodeParserTest PRIVATE Qt5::Core)
endif()
add_test(NAME GCodeParserTest COMMAND FanucGCodeParserTest)

add_executable(FanucGCodeModalTest
    tests/GCodeModalTest.cpp
    src/core/GCodeParser.cpp
    src/core/GCodeModalEngine.cpp
    src/model/GCodeBlock.h
    src/model/ModalState.h
)
target_include_directories(FanucGCodeModalTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucGCodeModalTest PRIVATE Qt6::Core)
else()
    target_link_libraries(FanucGCodeModalTest PRIVATE Qt5::Core)
endif()
add_test(NAME GCodeModalTest COMMAND FanucGCodeModalTest)

add_executable(FanucMotionPlannerTest
    tests/MotionPlannerTest.cpp
    src/core/GCodeModalEngine.cpp
    src/core/GCodeParser.cpp
    src/core/MotionPlanner.cpp
    src/model/GCodeBlock.h
    src/model/ModalState.h
    src/model/MotionSegment.h
)
target_include_directories(FanucMotionPlannerTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucMotionPlannerTest PRIVATE Qt6::Core Qt6::Gui)
else()
    target_link_libraries(FanucMotionPlannerTest PRIVATE Qt5::Core Qt5::Gui)
endif()
add_test(NAME MotionPlannerTest COMMAND FanucMotionPlannerTest)

add_executable(FanucCoordinateTransformTest
    tests/CoordinateTransformTest.cpp
    src/core/CoordinateTransform.cpp
    src/core/CoordinateTransform.h
)
target_include_directories(FanucCoordinateTransformTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucCoordinateTransformTest PRIVATE Qt6::Gui)
else()
    target_link_libraries(FanucCoordinateTransformTest PRIVATE Qt5::Gui)
endif()
add_test(NAME CoordinateTransformTest COMMAND FanucCoordinateTransformTest)

add_executable(FanucPlcSimulatorTest
    tests/PlcSimulatorTest.cpp
    src/core/PlcSimulator.cpp
    src/core/PlcSimulator.h
    src/model/ModalState.h
    src/model/PlcState.h
)
target_include_directories(FanucPlcSimulatorTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucPlcSimulatorTest PRIVATE Qt6::Core)
else()
    target_link_libraries(FanucPlcSimulatorTest PRIVATE Qt5::Core)
endif()
add_test(NAME PlcSimulatorTest COMMAND FanucPlcSimulatorTest)

add_executable(FanucMacroEngineTest
    tests/MacroEngineTest.cpp
    src/core/GCodeParser.cpp
    src/core/MacroEngine.cpp
    src/model/GCodeBlock.h
)
target_include_directories(FanucMacroEngineTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucMacroEngineTest PRIVATE Qt6::Core)
else()
    target_link_libraries(FanucMacroEngineTest PRIVATE Qt5::Core)
endif()
add_test(NAME MacroEngineTest COMMAND FanucMacroEngineTest)

add_executable(FanucSampleProgramsTest
    tests/SampleProgramsTest.cpp
    src/core/GCodeModalEngine.cpp
    src/core/GCodeParser.cpp
    src/core/MacroEngine.cpp
    src/core/MotionPlanner.cpp
    src/data/SamplePrograms.cpp
    src/model/GCodeBlock.h
    src/model/ModalState.h
    src/model/MotionSegment.h
    src/model/ProgramRecord.h
)
target_include_directories(FanucSampleProgramsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucSampleProgramsTest PRIVATE Qt6::Core Qt6::Gui)
else()
    target_link_libraries(FanucSampleProgramsTest PRIVATE Qt5::Core Qt5::Gui)
endif()
add_test(NAME SampleProgramsTest COMMAND FanucSampleProgramsTest)

add_executable(FanucMachineControllerResetTest
    tests/MachineControllerResetTest.cpp
    src/core/CoordinateTransform.cpp
    src/core/GCodeModalEngine.cpp
    src/core/GCodeParser.cpp
    src/core/MacroEngine.cpp
    src/core/MachineController.cpp
    src/core/MotionPlanner.cpp
    src/core/PlcSimulator.cpp
    src/data/AppDatabase.cpp
    src/data/SamplePrograms.cpp
    src/model/GCodeBlock.h
    src/model/ModalState.h
    src/model/MotionSegment.h
    src/model/PlcState.h
    src/model/ProgramRecord.h
    src/model/ToolOffset.h
    src/model/WorkOffset.h
)
target_include_directories(FanucMachineControllerResetTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucMachineControllerResetTest PRIVATE Qt6::Core Qt6::Gui Qt6::Sql)
else()
    target_link_libraries(FanucMachineControllerResetTest PRIVATE Qt5::Core Qt5::Gui Qt5::Sql)
endif()
add_test(NAME MachineControllerResetTest COMMAND FanucMachineControllerResetTest)

add_executable(FanucSingleBlockExecutionTest
    tests/SingleBlockExecutionTest.cpp
    src/core/CoordinateTransform.cpp
    src/core/GCodeModalEngine.cpp
    src/core/GCodeParser.cpp
    src/core/MacroEngine.cpp
    src/core/MachineController.cpp
    src/core/MotionPlanner.cpp
    src/core/PlcSimulator.cpp
    src/data/AppDatabase.cpp
    src/data/SamplePrograms.cpp
    src/model/GCodeBlock.h
    src/model/ModalState.h
    src/model/MotionSegment.h
    src/model/PlcState.h
    src/model/ProgramRecord.h
    src/model/ToolOffset.h
    src/model/WorkOffset.h
    src/ui/MachinePanel.cpp
    src/ui/MachinePanel.h
    src/ui/PanelStyle.h
    src/ui/RotaryDial.cpp
    src/ui/RotaryDial.h
)
target_include_directories(FanucSingleBlockExecutionTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(Qt6_FOUND)
    target_link_libraries(FanucSingleBlockExecutionTest PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Widgets)
else()
    target_link_libraries(FanucSingleBlockExecutionTest PRIVATE Qt5::Core Qt5::Gui Qt5::Sql Qt5::Widgets)
endif()
add_test(NAME SingleBlockExecutionTest COMMAND FanucSingleBlockExecutionTest)

if(MSVC)
    target_compile_options(FanucOiTF_QtDemo PRIVATE /utf-8)
endif()

if(Qt6_FOUND)
    target_link_libraries(FanucOiTF_QtDemo PRIVATE Qt6::Widgets Qt6::Sql)
else()
    target_link_libraries(FanucOiTF_QtDemo PRIVATE Qt5::Widgets Qt5::Sql)
endif()
