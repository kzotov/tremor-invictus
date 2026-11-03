// How much bots want to fight with each weapon they hold with ammo (the
// weapon weights; names as in weapons.c), the bot audit's weapon scores
// (bots-audit BOT-02). Our own data. The game's own table (ai_dmq3.c
// botWeapons) chooses by range first; this is its fallback.

weight "Gauntlet"
{
	switch(INVENTORY_GAUNTLET)
	{
		case 1: return 0;
		default: return 10;
	}
}

weight "Machinegun"
{
	switch(INVENTORY_MACHINEGUN)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_BULLETS)
			{
				case 1: return 0;
				default: return 50;
			}
		}
	}
}

weight "Shotgun"
{
	switch(INVENTORY_SHOTGUN)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_SHELLS)
			{
				case 1: return 0;
				default: return 55;
			}
		}
	}
}

weight "Tri-bolt"
{
	switch(INVENTORY_GRENADELAUNCHER)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_GRENADES)
			{
				case 1: return 0;
				default: return 70;
			}
		}
	}
}

weight "Rocket Launcher"
{
	switch(INVENTORY_ROCKETLAUNCHER)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_ROCKETS)
			{
				case 1: return 0;
				default: return 90;
			}
		}
	}
}

weight "Lightning Gun"
{
	switch(INVENTORY_LIGHTNING)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_LIGHTNINGAMMO)
			{
				case 1: return 0;
				default: return 90;
			}
		}
	}
}

weight "Railgun"
{
	switch(INVENTORY_RAILGUN)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_SLUGS)
			{
				case 1: return 0;
				default: return 95;
			}
		}
	}
}

weight "Super Nailgun"
{
	switch(INVENTORY_PLASMAGUN)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_CELLS)
			{
				case 1: return 0;
				default: return 85;
			}
		}
	}
}

weight "BFG10K"
{
	switch(INVENTORY_BFG10K)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_BFGAMMO)
			{
				case 1: return 0;
				default: return 95;
			}
		}
	}
}

weight "Grappling Hook"
{
	switch(INVENTORY_GRAPPLINGHOOK)
	{
		case 1: return 0;
		default: return 0;
	}
}

weight "Heavy Machinegun"
{
	switch(INVENTORY_HMG)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_BULLETS)
			{
				case 1: return 0;
				default: return 75;
			}
		}
	}
}

weight "Super Shotgun"
{
	switch(INVENTORY_SSG)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_SHELLS)
			{
				case 1: return 0;
				default: return 75;
			}
		}
	}
}

weight "Nailgun"
{
	switch(INVENTORY_NAILGUN)
	{
		case 1: return 0;
		default:
		{
			switch(INVENTORY_CELLS)
			{
				case 1: return 0;
				default: return 65;
			}
		}
	}
}
