export default function Navbar() {
  return (
    <header className="bg-white max-h-32 shadow px-10 py-10 flex justify-between items-center top-0 sticky navbar-default navbar-fixed-top !z-40">
      <div>
        <h1 className="hidden font-bold text-black uppercase pt-2">Dashboard</h1>
      </div>
      <div className="flex justify-end">
        <div className="flex flex-col gap-4">
          <div className="flex h-10 justify-end items-center">
            <img
              alt="Tania Andrew"
              src="https://images.unsplash.com/photo-1633332755192-727a05c4013d?ixlib=rb-1.2.1&amp;ixid=MnwxMjA3fDB8MHxwaG90by1wYWdlfHx8fGVufDB8fHx8&amp;auto=format&amp;fit=crop&amp;w=1480&amp;q=80"
              className="h-16  rounded-full object-cover object-center"
            />
            <div className="ml-2">
              Tania Andrew
              <hr className="h-[1.5px] w-full bg-gray-300 border-0" />
              Admin
            </div>
          </div>
        </div>
      </div>
    </header>
  );
}
