cmake_minimum_required(VERSION 2.4)
PROJECT (csm C CXX)
cmake_policy(SET CMP0004 OLD)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/json-c)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/options)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/egsl)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/csm)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/csm/icp)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/csm/gpm)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/csm/hsm)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include/csm/mbicp)
SET(local_prefix "local_config/local-")
FIND_PROGRAM(HOSTNAME hostname PATHS $ENV{PATH})
IF(HOSTNAME)
	# TODO: hostname does not accept -s on cygwin
	IF(CYGWIN OR WIN32)
		SET(hostname_params "")
	ELSE(CYGWIN OR WIN32)
		SET(hostname_params "-s")
	ENDIF(CYGWIN OR WIN32)
	
	EXEC_PROGRAM(${HOSTNAME} ARGS ${hostname_params} OUTPUT_VARIABLE host)

	SET(local "${local_prefix}${host}.cmake")
	
	SET(extension "$ENV{CSM_CONF}")
	IF(extension)
		SET(local "${local_prefix}${host}-${extension}.cmake")
	ENDIF(extension)
	
	IF(EXISTS ${local})
		MESSAGE(STATUS "Including local configuration ${local}.")
		INCLUDE(${local})
	ELSE(EXISTS ${local})
		MESSAGE(STATUS "Create a file named '${local}' if you want to add options.")
	ENDIF(EXISTS ${local})
ELSE(HOSTNAME)
	MESSAGE(STATUS "Program `hostname` not found. (don't worry)")
ENDIF(HOSTNAME)


ENABLE_TESTING()
SUBDIRS(sm)

find_package(catkin REQUIRED)
catkin_package(
  INCLUDE_DIRS include
  LIBRARIES csm csm-static
)
