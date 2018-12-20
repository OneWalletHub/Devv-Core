/*
 * Example a Java integration using Devv Proposals and Transactions
 * to communicate with the Devv blockchain through Protobufs and JNI.
 *
 * @copywrite  2018 Devvio Inc
 */

package jni.devv.java;

import org.zeromq.ZMQ;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import proto.devv.Devv;
import proto.devv.Devv.Envelope;
import proto.devv.Devv.FinalBlock;
import proto.devv.Devv.Proposal;
import proto.devv.Devv.Transaction;
import proto.devv.Devv.Transfer;
import proto.devv.Devv.RepeaterRequest;
import proto.devv.Devv.RepeaterResponse;

public class DevvTestMain {

	static final public String ADDR_1 = "2102514038DA1905561BF9043269B8515C1E7C4E79B011291B4CBED5B18DAECB71E4";
	static final public String ADDR_1_KEY = "-----BEGIN ENCRYPTED PRIVATE KEY-----\\nMIHeMEkGCSqGSIb3DQEFDTA8MBsGCSqGSIb3DQEFDDAOBAh8ZjsVLSSMlgICCAAw\\nHQYJYIZIAWUDBAECBBBcQGPzeImxYCj108A3DM8CBIGQjDAPienFMoBGwid4R5BL\\nwptYiJbxAfG7EtQ0SIXqks8oBIDre0n7wd+nq3NRecDANwSzCdyC3IeCdKx87eEf\\nkspgo8cjNlEKUVVg9NR2wbVp5+UClmQH7LCsZB5HAxF4ijHaSDNe5hD6gOZqpXi3\\nf5eNexJ2fH+OqKd5T9kytJyoK3MAXFS9ckt5JxRlp6bf\\n-----END ENCRYPTED PRIVATE KEY-----";
	static final public String ADDR_2 = "2102E14466DC0E5A3E6EBBEAB5DD24ABE950E44EF2BEB509A5FD113460414A6EFAB4";
	static final public String SOME_NONCE_HEXSTR = "00112233445566778899AABBCCDDEEFF";
	static final public String ANNOUNCER_URL = "tcp://*:55706";
	static final public String REPEATER_URL = "tcp://*:55806";
	static final public String PK_AES_PASS = "password";

	static final public int CHECK_TRANSACTION = 9;
	static final public int GET_BLOCK_AS_PBUF = 5;

	private final static char[] hexArray = "0123456789ABCDEF".toCharArray();
	public static String bytesToHex(byte[] bytes) {
	    char[] hexChars = new char[bytes.length * 2];
	    for ( int j = 0; j < bytes.length; j++ ) {
	        int v = bytes[j] & 0xFF;
	        hexChars[j * 2] = hexArray[v >>> 4];
	        hexChars[j * 2 + 1] = hexArray[v & 0x0F];
	    }
	    return new String(hexChars);
	}

	static public byte[] hexStringToByteArray(String s) {
	    int len = s.length();
	    byte[] data = new byte[len / 2];
	    for (int i = 0; i < len; i += 2) {
	        data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
	                             + Character.digit(s.charAt(i+1), 16));
	    }
	    return data;
	}

	/** use Devv shared library to Sign a transaction
	 *
	 * @params tx - the unsigned transaction in binary form
	 * @params keyPass - the AES password for the key
	 * @params privateKey - an ECDSA private key, AES encrypted with ASCII armor
	 * @return the signed transaction in canonical form
	 */
	public native byte[] SignTransaction(byte[] tx, String keyPass, byte[] privateKey);

	/** Use Devv shared library to Create a proposal
	 *
	 * @params oracle - the fully qualified name of the oracle to invoke
	 * @params data - the raw data to provide this oracle
	 * @params keyAddress - the addresses corresponding to this private key
	 * @params keyPass - the AES password for the key
	 * @params privateKey - an ECDSA private key, AES encrypted with ASCII armor
	 * @return a binary proposal including the signature(s) as needed
	 */
	public native byte[] CreateProposal(String oracle, byte[] data, String keyAddress, String keyPass, byte[] privateKey);

	/** Generate a transaction
	 *
	 * @params sender - the transaction sender/signer address
	 * @params receiver - the transaction recipient address
	 * @params coin - the type of coin to send
	 * @params amount - the amount of this coin to send
	 * @params delay - a delay (in seconds) before this transaction settles and can be reversed
	 * @params nonce - arbitrary contextual binary data associated with this transaction
	 * @params sig - the sender's signature using the hash of rest of this transaction in canonical form as a message digest
	 * @note use the JNI SignTransaction method to generate a signed version of this transaction for the given key
	 * @return a binary proposal including the signature(s) as needed
	 */
	public Transaction getTransaction(ByteString sender, ByteString receiver, long coin, long amount, long delay, byte[] nonce, byte[] sig) {
		Transfer xfer = Transfer.newBuilder()
				.setAddress(sender)
				.setAmount(-1)
				.setCoin(0)
				.setDelay(0)
				.build();
		Transfer xfer2 = Transfer.newBuilder()
				.setAddress(receiver)
				.setAmount(1)
				.setCoin(0)
				.setDelay(0)
				.build();
		Transaction tx = Transaction.newBuilder()
				.addXfers(xfer)
				.addXfers(xfer2)
				.setOperationValue(2)
				.setNonce(ByteString.copyFrom(nonce))
				.setSig(ByteString.copyFrom(sig))
				.build();
		return tx;
	}

	public Proposal getDataProposal(ByteString sender, ByteString data) {
		final String DATA_ORACLE = "io.devv.data";
		byte[] signed_raw = CreateProposal(DATA_ORACLE, data.toByteArray(), ADDR_1, PK_AES_PASS, hexStringToByteArray(ADDR_1_KEY));
		Proposal prop = Proposal.newBuilder()
				.setOraclename(DATA_ORACLE)
				.setData(ByteString.copyFrom(signed_raw))
				.build();
		return prop;
	}

	public static void main(String[] args) {
		try {
			System.loadLibrary("devvjni");

			ZMQ.Context context = ZMQ.context(1);

	        ZMQ.Socket requester = context.socket(ZMQ.REQ);
	        requester.connect(ANNOUNCER_URL);

	        ZMQ.Socket repeater = context.socket(ZMQ.REQ);
	        repeater.connect(REPEATER_URL);

			DevvTestMain test = new DevvTestMain();
			Transaction tx = test.getTransaction(ByteString.copyFrom(hexStringToByteArray(ADDR_1)),
					ByteString.copyFrom(hexStringToByteArray(ADDR_2)),
					0L, 1L, 0L,
					hexStringToByteArray(SOME_NONCE_HEXSTR),
					hexStringToByteArray(ADDR_1));
			byte[] signed = test.SignTransaction(tx.toByteArray(), PK_AES_PASS, hexStringToByteArray(ADDR_1_KEY));
			try {
			  Transaction tx2 = Transaction.parseFrom(signed);
			  Envelope env = Envelope.newBuilder().addTxs(tx2).build();
			  requester.send(env.toByteArray(), 0);
			  byte[] reply = requester.recv(0);
	          System.out.println("Received " + new String(reply));

	          RepeaterRequest request = RepeaterRequest.newBuilder()
	        		  .setTimestamp(System.currentTimeMillis())
	        		  .setOperation(CHECK_TRANSACTION)
	        		  .setUri("devv://shard-1/0/"+bytesToHex(tx2.getSig().toByteArray()))
	        		  .build();
	          repeater.send(request.toByteArray(), 0);
	          RepeaterResponse response = RepeaterResponse.parseFrom(repeater.recv(0));
	          if (response.getReturnCode() == 0) {
	        	System.out.println("Success: Transaction is in block #"+response.getRawResponse().toString());
	        	RepeaterRequest blockRequest = RepeaterRequest.newBuilder()
		        		  .setTimestamp(System.currentTimeMillis())
		        		  .setOperation(GET_BLOCK_AS_PBUF)
		        		  .setUri("devv://shard-1/"+response.getRawResponse().toString())
		        		  .build();
		        repeater.send(blockRequest.toByteArray(), 0);
		        RepeaterResponse blockResponse = RepeaterResponse.parseFrom(repeater.recv(0));
		        if (blockResponse.getReturnCode() == 0) {
		          FinalBlock block = FinalBlock.parseFrom(blockResponse.getRawResponse());
		          for (Devv.Transaction one_tx : block.getTxsList()) {
		            if (one_tx.getSig() == tx2.getSig()) {
		            	System.out.println("Found the transaction with nonce: "+one_tx.getNonce());
		            }
		          }
		        } else {
		          System.out.println("Error (#"+response.getReturnCode()+"): "+response.getMessage());
		        }
	          } else {
	        	System.out.println("Error (#"+response.getReturnCode()+"): "+response.getMessage());
	          }
			} catch (InvalidProtocolBufferException ipbe) {
				System.err.println("InvalidProtocolBufferException: "+ipbe.getMessage());
			}
	        requester.close();

	        context.term();
		} catch (Exception e) {
			System.err.println(e.getClass()+": "+e.getMessage());
		}
	}
}
