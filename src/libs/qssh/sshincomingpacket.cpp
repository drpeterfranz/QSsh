/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: http://www.qt-project.org/
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**************************************************************************/

#include "sshincomingpacket_p.h"

#include "ssh_global.h"
#include "sshbotanconversions_p.h"
#include "sshcapabilities_p.h"
#include "sshlogging_p.h"

namespace QSsh {
namespace Internal {

const QByteArray SshIncomingPacket::ExitStatusType("exit-status");
const QByteArray SshIncomingPacket::ExitSignalType("exit-signal");
const QByteArray SshIncomingPacket::ForwardedTcpIpType("forwarded-tcpip");

SshIncomingPacket::SshIncomingPacket() : m_serverSeqNr(0) { }

quint32 SshIncomingPacket::cipherBlockSize() const
{
    return qMax(m_decrypter.cipherBlockSize(), 8U);
}

quint32 SshIncomingPacket::macLength() const
{
    return m_decrypter.macLength();
}

void SshIncomingPacket::recreateKeys(const SshKeyExchange &keyExchange)
{
    m_decrypter.recreateKeys(keyExchange);
}

void SshIncomingPacket::reset()
{
    clear();
    m_serverSeqNr = 0;
    m_decrypter.clearKeys();
}

void SshIncomingPacket::consumeData(QByteArray &newData)
{
    qCDebug(sshLog, "%s: current data size = %d, new data size = %d",
        Q_FUNC_INFO, int(m_data.size()), int(newData.size()));

    if (isComplete() || newData.isEmpty())
        return;

    /*
     * Until we have reached the minimum packet size, we cannot decrypt the
     * length field.
     */
    const quint32 minSize = minPacketSize();
    if (currentDataSize() < minSize) {
        const int bytesToTake
            = qMin<quint32>(minSize - currentDataSize(), newData.size());
        moveFirstBytes(m_data, newData, bytesToTake);
        qCDebug(sshLog, "Took %d bytes from new data", bytesToTake);
        if (currentDataSize() < minSize)
            return;
    }

    if (4 + length() + macLength() < currentDataSize())
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR, "Server sent invalid packet.");

    const int bytesToTake
        = qMin<quint32>(length() + 4 + macLength() - currentDataSize(),
              newData.size());
    moveFirstBytes(m_data, newData, bytesToTake);
    qCDebug(sshLog, "Took %d bytes from new data", bytesToTake);
    if (isComplete()) {
        qCDebug(sshLog, "Message complete. Overall size: %u, payload size: %u",
            int(m_data.size()), m_length - paddingLength() - 1);
        decrypt();
        ++m_serverSeqNr;
    }
}

void SshIncomingPacket::decrypt()
{
    Q_ASSERT(isComplete());
    const quint32 netDataLength = length() + 4;
    m_decrypter.decrypt(m_data, cipherBlockSize(),
        netDataLength - cipherBlockSize());
    const QByteArray &mac = m_data.mid(netDataLength, macLength());
    if (mac != generateMac(m_decrypter, m_serverSeqNr)) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_MAC_ERROR,
                           "Message authentication failed.");
    }
}

void SshIncomingPacket::moveFirstBytes(QByteArray &target, QByteArray &source,
    int n)
{
    target.append(source.left(n));
    source.remove(0, n);
}

SshKeyExchangeInit SshIncomingPacket::extractKeyExchangeInitData() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_KEXINIT);

    SshKeyExchangeInit exchangeData;
    try {
        quint32 offset = TypeOffset + 1;
        std::memcpy(exchangeData.cookie, &m_data.constData()[offset],
                    sizeof exchangeData.cookie);
        offset += sizeof exchangeData.cookie;
        exchangeData.keyAlgorithms
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.serverHostKeyAlgorithms
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.encryptionAlgorithmsClientToServer
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.encryptionAlgorithmsServerToClient
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.macAlgorithmsClientToServer
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.macAlgorithmsServerToClient
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.compressionAlgorithmsClientToServer
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.compressionAlgorithmsServerToClient
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.languagesClientToServer
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.languagesServerToClient
            = SshPacketParser::asNameList(m_data, &offset);
        exchangeData.firstKexPacketFollows
            = SshPacketParser::asBool(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
            "Key exchange failed: Server sent invalid SSH_MSG_KEXINIT packet.");
    }
    return exchangeData;
}

static void getHostKeySpecificReplyData(SshKeyExchangeReply &replyData,
                                        const QByteArray &hostKeyAlgo, const QByteArray &input)
{
    quint32 offset = 0;
    if (hostKeyAlgo == SshCapabilities::PubKeyDss || hostKeyAlgo == SshCapabilities::PubKeyRsa) {
        // DSS: p and q, RSA: e and n
        replyData.hostKeyParameters << SshPacketParser::asBigInt(input, &offset);
        replyData.hostKeyParameters << SshPacketParser::asBigInt(input, &offset);

        // g and y
        if (hostKeyAlgo == SshCapabilities::PubKeyDss) {
            replyData.hostKeyParameters << SshPacketParser::asBigInt(input, &offset);
            replyData.hostKeyParameters << SshPacketParser::asBigInt(input, &offset);
        }
    } else {
        QSSH_ASSERT_AND_RETURN(hostKeyAlgo.startsWith(SshCapabilities::PubKeyEcdsaPrefix));
        if (SshPacketParser::asString(input, &offset)
                != hostKeyAlgo.mid(11)) { // Without "ecdsa-sha2-" prefix.
            throw SshPacketParseException();
        }
        replyData.q = SshPacketParser::asString(input, &offset);
    }
}

static QByteArray &padToWidth(QByteArray &data, int targetWidth)
{
    return data.prepend(QByteArray(targetWidth - data.size(), 0));
}

SshKeyExchangeReply SshIncomingPacket::extractKeyExchangeReply(const QByteArray &kexAlgo,
                                                               const QByteArray &hostKeyAlgo) const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_KEXDH_REPLY);

    try {
        SshKeyExchangeReply replyData;
        quint32 topLevelOffset = TypeOffset + 1;
        replyData.k_s = SshPacketParser::asString(m_data, &topLevelOffset);
        quint32 k_sOffset = 0;
        if (SshPacketParser::asString(replyData.k_s, &k_sOffset) != hostKeyAlgo)
            throw SshPacketParseException();
        getHostKeySpecificReplyData(replyData, hostKeyAlgo, replyData.k_s.mid(k_sOffset));

        if (kexAlgo == SshCapabilities::DiffieHellmanGroup1Sha1
                || kexAlgo == SshCapabilities::DiffieHellmanGroup14Sha1) {
            replyData.f = SshPacketParser::asBigInt(m_data, &topLevelOffset);
        } else {
            QSSH_ASSERT_AND_RETURN_VALUE(kexAlgo.startsWith(SshCapabilities::EcdhKexNamePrefix),
                                         SshKeyExchangeReply());
            replyData.q_s = SshPacketParser::asString(m_data, &topLevelOffset);
        }
        const QByteArray fullSignature = SshPacketParser::asString(m_data, &topLevelOffset);
        quint32 sigOffset = 0;
        if (SshPacketParser::asString(fullSignature, &sigOffset) != hostKeyAlgo)
            throw SshPacketParseException();
        replyData.signatureBlob = SshPacketParser::asString(fullSignature, &sigOffset);
        if (hostKeyAlgo.startsWith(SshCapabilities::PubKeyEcdsaPrefix)) {
            // Botan's PK_Verifier wants the signature in this format.
            quint32 blobOffset = 0;
            const Botan::BigInt r = SshPacketParser::asBigInt(replyData.signatureBlob, &blobOffset);
            const Botan::BigInt s = SshPacketParser::asBigInt(replyData.signatureBlob, &blobOffset);
            const int width = SshCapabilities::ecdsaIntegerWidthInBytes(hostKeyAlgo);
            QByteArray encodedR = convertByteArray(Botan::BigInt::encode(r));
            replyData.signatureBlob = padToWidth(encodedR, width);
            QByteArray encodedS = convertByteArray(Botan::BigInt::encode(s));
            replyData.signatureBlob += padToWidth(encodedS, width);
        }
        replyData.k_s.prepend(m_data.mid(TypeOffset + 1, 4));
        return replyData;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
            "Key exchange failed: "
            "Server sent invalid key exchange reply packet.");
    }
}

SshDisconnect SshIncomingPacket::extractDisconnect() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_DISCONNECT);

    SshDisconnect msg;
    try {
        quint32 offset = TypeOffset + 1;
        msg.reasonCode = SshPacketParser::asUint32(m_data, &offset);
        msg.description = SshPacketParser::asUserString(m_data, &offset);
        msg.language = SshPacketParser::asString(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_DISCONNECT.");
    }

    return msg;
}

SshUserAuthBanner SshIncomingPacket::extractUserAuthBanner() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_USERAUTH_BANNER);

    try {
        SshUserAuthBanner msg;
        quint32 offset = TypeOffset + 1;
        msg.message = SshPacketParser::asUserString(m_data, &offset);
        msg.language = SshPacketParser::asString(m_data, &offset);
        return msg;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_USERAUTH_BANNER.");
    }
}

SshUserAuthInfoRequestPacket SshIncomingPacket::extractUserAuthInfoRequest() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_USERAUTH_INFO_REQUEST);

    try {
        SshUserAuthInfoRequestPacket msg;
        quint32 offset = TypeOffset + 1;
        msg.name = SshPacketParser::asUserString(m_data, &offset);
        msg.instruction = SshPacketParser::asUserString(m_data, &offset);
        msg.languageTag = SshPacketParser::asString(m_data, &offset);
        const quint32 promptCount = SshPacketParser::asUint32(m_data, &offset);
        msg.prompts.reserve(promptCount);
        msg.echos.reserve(promptCount);
        for (quint32 i = 0; i < promptCount; ++i) {
            msg.prompts << SshPacketParser::asUserString(m_data, &offset);
            msg.echos << SshPacketParser::asBool(m_data, &offset);
        }
        return msg;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_USERAUTH_INFO_REQUEST.");
    }
}

SshUserAuthPkOkPacket SshIncomingPacket::extractUserAuthPkOk() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_USERAUTH_PK_OK);

    try {
        SshUserAuthPkOkPacket msg;
        quint32 offset = TypeOffset + 1;
        msg.algoName= SshPacketParser::asString(m_data, &offset);
        msg.keyBlob = SshPacketParser::asString(m_data, &offset);
        return msg;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_USERAUTH_PK_OK.");
    }
}

SshDebug SshIncomingPacket::extractDebug() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_DEBUG);

    try {
        SshDebug msg;
        quint32 offset = TypeOffset + 1;
        msg.display = SshPacketParser::asBool(m_data, &offset);
        msg.message = SshPacketParser::asUserString(m_data, &offset);
        msg.language = SshPacketParser::asString(m_data, &offset);
        return msg;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_DEBUG.");
    }
}

SshRequestSuccess SshIncomingPacket::extractRequestSuccess() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_REQUEST_SUCCESS);

    try {
        SshRequestSuccess msg;
        quint32 offset = TypeOffset + 1;
        msg.bindPort = SshPacketParser::asUint32(m_data, &offset);
        return msg;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_REQUEST_SUCCESS.");
    }
}

SshUnimplemented SshIncomingPacket::extractUnimplemented() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_UNIMPLEMENTED);

    try {
        SshUnimplemented msg;
        quint32 offset = TypeOffset + 1;
        msg.invalidMsgSeqNr = SshPacketParser::asUint32(m_data, &offset);
        return msg;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_UNIMPLEMENTED.");
    }
}

SshChannelOpenGeneric SshIncomingPacket::extractChannelOpen() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_OPEN);

    try {
        SshChannelOpenGeneric channelOpen;
        quint32 offset = TypeOffset + 1;
        channelOpen.channelType = SshPacketParser::asString(m_data, &offset);
        channelOpen.commonData.remoteChannel = SshPacketParser::asUint32(m_data, &offset);
        channelOpen.commonData.remoteWindowSize = SshPacketParser::asUint32(m_data, &offset);
        channelOpen.commonData.remoteMaxPacketSize = SshPacketParser::asUint32(m_data, &offset);
        channelOpen.typeSpecificData = m_data.mid(offset, length() - paddingLength() - offset
                                                  + int(sizeof m_length));
        return channelOpen;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Server sent invalid SSH_MSG_CHANNEL_OPEN packet.");
    }
}

SshChannelOpenForwardedTcpIp SshIncomingPacket::extractChannelOpenForwardedTcpIp(
        const SshChannelOpenGeneric &genericData)
{
    try {
        SshChannelOpenForwardedTcpIp specificData;
        specificData.common = genericData.commonData;
        quint32 offset = 0;
        specificData.remoteAddress = SshPacketParser::asString(genericData.typeSpecificData,
                                                               &offset);
        specificData.remotePort = SshPacketParser::asUint32(genericData.typeSpecificData, &offset);
        specificData.originatorAddress = SshPacketParser::asString(genericData.typeSpecificData,
                                                                   &offset);
        specificData.originatorPort = SshPacketParser::asUint32(genericData.typeSpecificData,
                                                                &offset);
        return specificData;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Server sent invalid SSH_MSG_CHANNEL_OPEN packet.");
    }
}

SshChannelOpenX11 SshIncomingPacket::extractChannelOpenX11(const SshChannelOpenGeneric &genericData)
{
    try {
        SshChannelOpenX11 specificData;
        specificData.common = genericData.commonData;
        quint32 offset = 0;
        specificData.originatorAddress = SshPacketParser::asString(genericData.typeSpecificData,
                                                                   &offset);
        specificData.originatorPort = SshPacketParser::asUint32(genericData.typeSpecificData,
                                                                &offset);
        return specificData;
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Server sent invalid SSH_MSG_CHANNEL_OPEN packet.");
    }
}

SshChannelOpenFailure SshIncomingPacket::extractChannelOpenFailure() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_OPEN_FAILURE);

    SshChannelOpenFailure openFailure;
    try {
        quint32 offset = TypeOffset + 1;
        openFailure.localChannel = SshPacketParser::asUint32(m_data, &offset);
        openFailure.reasonCode = SshPacketParser::asUint32(m_data, &offset);
        openFailure.reasonString = QString::fromLocal8Bit(SshPacketParser::asString(m_data, &offset));
        openFailure.language = SshPacketParser::asString(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Server sent invalid SSH_MSG_CHANNEL_OPEN_FAILURE packet.");
    }
    return openFailure;
}

SshChannelOpenConfirmation SshIncomingPacket::extractChannelOpenConfirmation() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_OPEN_CONFIRMATION);

    SshChannelOpenConfirmation confirmation;
    try {
        quint32 offset = TypeOffset + 1;
        confirmation.localChannel = SshPacketParser::asUint32(m_data, &offset);
        confirmation.remoteChannel = SshPacketParser::asUint32(m_data, &offset);
        confirmation.remoteWindowSize = SshPacketParser::asUint32(m_data, &offset);
        confirmation.remoteMaxPacketSize = SshPacketParser::asUint32(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Server sent invalid SSH_MSG_CHANNEL_OPEN_CONFIRMATION packet.");
    }
    return confirmation;
}

SshChannelWindowAdjust SshIncomingPacket::extractWindowAdjust() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_WINDOW_ADJUST);

    SshChannelWindowAdjust adjust;
    try {
        quint32 offset = TypeOffset + 1;
        adjust.localChannel = SshPacketParser::asUint32(m_data, &offset);
        adjust.bytesToAdd = SshPacketParser::asUint32(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_CHANNEL_WINDOW_ADJUST packet.");
    }
    return adjust;
}

SshChannelData SshIncomingPacket::extractChannelData() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_DATA);

    SshChannelData data;
    try {
        quint32 offset = TypeOffset + 1;
        data.localChannel = SshPacketParser::asUint32(m_data, &offset);
        data.data = SshPacketParser::asString(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_CHANNEL_DATA packet.");
    }
    return data;
}

SshChannelExtendedData SshIncomingPacket::extractChannelExtendedData() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_EXTENDED_DATA);

    SshChannelExtendedData data;
    try {
        quint32 offset = TypeOffset + 1;
        data.localChannel = SshPacketParser::asUint32(m_data, &offset);
        data.type = SshPacketParser::asUint32(m_data, &offset);
        data.data = SshPacketParser::asString(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_CHANNEL_EXTENDED_DATA packet.");
    }
    return data;
}

SshChannelExitStatus SshIncomingPacket::extractChannelExitStatus() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_REQUEST);

    SshChannelExitStatus exitStatus;
    try {
        quint32 offset = TypeOffset + 1;
        exitStatus.localChannel = SshPacketParser::asUint32(m_data, &offset);
        const QByteArray &type = SshPacketParser::asString(m_data, &offset);
        Q_ASSERT(type == ExitStatusType);
        Q_UNUSED(type);
        if (SshPacketParser::asBool(m_data, &offset))
            throw SshPacketParseException();
        exitStatus.exitStatus = SshPacketParser::asUint32(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid exit-status packet.");
    }
    return exitStatus;
}

SshChannelExitSignal SshIncomingPacket::extractChannelExitSignal() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_REQUEST);

    SshChannelExitSignal exitSignal;
    try {
        quint32 offset = TypeOffset + 1;
        exitSignal.localChannel = SshPacketParser::asUint32(m_data, &offset);
        const QByteArray &type = SshPacketParser::asString(m_data, &offset);
        Q_ASSERT(type == ExitSignalType);
        Q_UNUSED(type);
        if (SshPacketParser::asBool(m_data, &offset))
            throw SshPacketParseException();
        exitSignal.signal = SshPacketParser::asString(m_data, &offset);
        exitSignal.coreDumped = SshPacketParser::asBool(m_data, &offset);
        exitSignal.error = SshPacketParser::asUserString(m_data, &offset);
        exitSignal.language = SshPacketParser::asString(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid exit-signal packet.");
    }
    return exitSignal;
}

quint32 SshIncomingPacket::extractRecipientChannel() const
{
    Q_ASSERT(isComplete());

    try {
        quint32 offset = TypeOffset + 1;
        return SshPacketParser::asUint32(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Server sent invalid packet.");
    }
}

QByteArray SshIncomingPacket::extractChannelRequestType() const
{
    Q_ASSERT(isComplete());
    Q_ASSERT(type() == SSH_MSG_CHANNEL_REQUEST);

    try {
        quint32 offset = TypeOffset + 1;
        SshPacketParser::asUint32(m_data, &offset);
        return SshPacketParser::asString(m_data, &offset);
    } catch (const SshPacketParseException &) {
        throw SSH_SERVER_EXCEPTION(SSH_DISCONNECT_PROTOCOL_ERROR,
            "Invalid SSH_MSG_CHANNEL_REQUEST packet.");
    }
}

void SshIncomingPacket::calculateLength() const
{
    Q_ASSERT(currentDataSize() >= minPacketSize());
    qCDebug(sshLog, "Length field before decryption: %d-%d-%d-%d", m_data.at(0) & 0xff,
        m_data.at(1) & 0xff, m_data.at(2) & 0xff, m_data.at(3) & 0xff);
    m_decrypter.decrypt(m_data, 0, cipherBlockSize());
    qCDebug(sshLog, "Length field after decryption: %d-%d-%d-%d", m_data.at(0) & 0xff, m_data.at(1) & 0xff, m_data.at(2) & 0xff, m_data.at(3) & 0xff);
    qCDebug(sshLog, "message type = %d", m_data.at(TypeOffset));
    m_length = SshPacketParser::asUint32(m_data, static_cast<quint32>(0));
    qCDebug(sshLog, "decrypted length is %u", m_length);
}

} // namespace Internal
} // namespace QSsh
