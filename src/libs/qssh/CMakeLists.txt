# select between static and shared library build
if(BUILD_SHARED_LIBS)
    set(LIBTYPE SHARED)
    set(DEFINITIONS QTCSSH_LIBRARY)
else()
    set(LIBTYPE STATIC)
    set(DEFINITIONS _MSC_VER)
endif()

if (MSVC)
    # to create a .lib file
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(SOURCES
    sshsendfacility.cpp
    sshremoteprocess.cpp
    sshpacketparser.cpp
    sshpacket.cpp
    sshoutgoingpacket.cpp
    sshkeygenerator.cpp
    sshkeyexchange.cpp
    sshincomingpacket.cpp
    sshcryptofacility.cpp
    sshconnection.cpp
    sshchannelmanager.cpp
    sshchannel.cpp
    sshcapabilities.cpp
    sftppacket.cpp
    sftpoutgoingpacket.cpp
    sftpoperation.cpp
    sftpincomingpacket.cpp
    sftpdefs.cpp
    sftpchannel.cpp
    sshremoteprocessrunner.cpp
    sshconnectionmanager.cpp
    sshkeypasswordretriever.cpp
    sftpfilesystemmodel.cpp
    sshdirecttcpiptunnel.cpp
    sshhostkeydatabase.cpp
    sshlogging.cpp
    sshtcpipforwardserver.cpp
    sshtcpiptunnel.cpp
    sshforwardedtcpiptunnel.cpp
    sshagent.cpp
    sshx11channel.cpp
    sshx11inforetriever.cpp
    opensshkeyfilereader.cpp
    qssh.qrc
    )

set(HEADERS
    opensshkeyfilereader_p.h
    sftpchannel.h
    sftpchannel_p.h
    sftpdefs.h
    sftpfilesystemmodel.h
    sftpincomingpacket_p.h
    sftpoperation_p.h
    sftpoutgoingpacket_p.h
    sftppacket_p.h
    ssh_global.h
    sshagent_p.h
    sshbotanconversions_p.h
    sshcapabilities_p.h
    sshchannel_p.h
    sshchannelmanager_p.h
    sshconnection.h
    sshconnection_p.h
    sshconnectionmanager.h
    sshcryptofacility_p.h
    sshdirecttcpiptunnel.h
    sshdirecttcpiptunnel_p.h
    ssherrors.h
    sshexception_p.h
    sshforwardedtcpiptunnel.h
    sshforwardedtcpiptunnel_p.h
    sshhostkeydatabase.h
    sshincomingpacket_p.h
    sshkeyexchange_p.h
    sshkeygenerator.h
    sshkeypasswordretriever_p.h
    sshlogging_p.h
    sshoutgoingpacket_p.h
    sshpacket_p.h
    sshpacketparser_p.h
    sshpseudoterminal.h
    sshremoteprocess.h
    sshremoteprocess_p.h
    sshremoteprocessrunner.h
    sshsendfacility_p.h
    sshtcpipforwardserver.h
    sshtcpipforwardserver_p.h
    sshtcpiptunnel_p.h
    sshx11channel_p.h
    sshx11displayinfo_p.h
    sshx11inforetriever_p.h
    )

add_library(QSsh ${LIBTYPE} "${SOURCES}" "${HEADERS}")

get_property(BOTAN_LIB GLOBAL PROPERTY BOTAN_LIB)
target_link_libraries(QSsh Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Widgets ${BOTAN_LIB})
target_compile_definitions(QSsh PRIVATE ${DEFINITIONS})

set_target_properties(QSsh
	PROPERTIES
	PUBLIC_HEADER "${HEADERS}"
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION}
	)
target_include_directories(QSsh PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include/QSsh>
        )
		  
#INSTALL RULES
if(WIN32)
    set(LIB_DIR ${CMAKE_INSTALL_BINDIR})
else()
    set(LIB_DIR ${CMAKE_INSTALL_LIBDIR})
endif()
install(TARGETS QSsh
	  EXPORT QSsh-targets
      RUNTIME DESTINATION ${LIB_DIR}
	  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/QSsh
	  )
