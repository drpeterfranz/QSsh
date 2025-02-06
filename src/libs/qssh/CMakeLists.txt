add_library(QSsh
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
    qssh.qrc)

target_link_libraries(QSsh Qt6::Core Qt6::Network Qt6::Widgets botan::botan)

# state that anybody linking to us needs to include the current source dir
target_include_directories(QSsh
          PRIVATE
          ${botan_INCLUDE_DIR}
          INTERFACE
		  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
		  $<INSTALL_INTERFACE:include>
          $<INSTALL_INTERFACE:include/qssh>
          )
		  
#INSTALL RULES
install(
  DIRECTORY .
  DESTINATION include
  COMPONENT headers
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
install(TARGETS QSsh DESTINATION lib EXPORT QSsh-targets)
