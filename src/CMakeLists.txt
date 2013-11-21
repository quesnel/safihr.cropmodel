include_directories(${CMAKE_SOURCE_DIR}/src ${VLE_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS})

link_directories(${VLE_LIBRARY_DIRS})

DeclareDevsDynamics(GenericCropModel "GenericCropModel.cpp;Global.hpp")
DeclareDevsDynamics(Meteo "Meteo.cpp")
DeclareDevsDynamics(MinimalistAI "MinimalistAI.cpp;AI.hpp;Global.hpp")
DeclareDevsDynamics(CompareDateAI "CompareDateAI.cpp;AI.hpp;Global.hpp")
