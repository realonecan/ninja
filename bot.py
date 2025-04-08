import random
import string
from telegram.ext import Updater, CommandHandler, MessageHandler, Filters, ConversationHandler

TOKEN = "7531254856:AAGGYb5eNB6P835UsofBPnhNEqoUmWGwlgU"  # Replace with your token

OPTION = 0  # Single state for simplicity

rooms = {}  # {room_num: {"public_pass": str, "owner_pass": str, "owner": str}}

def generate_username():
    suffix = ''.join(random.choices(string.ascii_lowercase + string.digits, k=4))
    return f"ninja-{suffix}"

def generate_room_num():
    while True:
        room = ''.join(random.choices(string.digits, k=5))
        if room not in rooms:
            return room

def generate_password():
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=8))

def start(update, context):
    update.message.reply_text(
        "Welcome! What’s your option?\n"
        "1. Get a username\n"
        "2. Create a room"
    )
    return OPTION

def handle_option(update, context):
    choice = update.message.text.strip()
    if choice == "1":
        username = generate_username()
        update.message.reply_text(f"Your username is: {username}\nUse it with 'ninja'.")
        return ConversationHandler.END
    elif choice == "2":
        room = generate_room_num()
        public_pass = generate_password()
        owner_pass = generate_password()
        username = generate_username()
        rooms[room] = {"public_pass": public_pass, "owner_pass": owner_pass, "owner": username}
        update.message.reply_text(
            f"Room created!\n"
            f"Room: {room}\n"
            f"Public password: {public_pass}\n"
            f"Owner password: {owner_pass}\n"
            f"Your username: {username}\n"
            f"Share the room number and public password to invite others."
        )
        return ConversationHandler.END
    else:
        update.message.reply_text("Invalid option. Choose 1 or 2.")
        return OPTION

def cancel(update, context):
    update.message.reply_text("Canceled.")
    return ConversationHandler.END

def main():
    updater = Updater(TOKEN, use_context=True)
    dp = updater.dispatcher

    conv_handler = ConversationHandler(
        entry_points=[CommandHandler("start", start)],
        states={
            OPTION: [MessageHandler(Filters.text & ~Filters.command, handle_option)],
        },
        fallbacks=[CommandHandler("cancel", cancel)],
    )
    dp.add_handler(conv_handler)

    updater.start_polling()
    updater.idle()

if __name__ == "__main__":
    main()